local lunajson = require 'lunajson'

local pdjson = pd.Class:new():register("pdjson")

-- json_data/jsonFileBuffer vivent sur self (par instance) -- pas de local de
-- module ici : plusieurs pdjson actifs dans le meme patch (ex. clip-io.pd +
-- composition-io.pd) ne doivent pas partager le meme document charge.
-- Verifie en conditions reelles (2026-07-19) : deux instances, deux read sur
-- des fichiers differents, un dump sur la premiere renvoyait le contenu de la
-- seconde avant ce correctif.

-- Fonction de copie de table (remplace unpack pour compatibilité)
local function copyTable(source)
  local dest = {}
  for i = 1, #source do
    dest[i] = source[i]
  end
  return dest
end


-- Fonction pour convertir les valeurs JSON en types compatibles PureData
local function sanitizeValue(value)
  if type(value) == "boolean" then
    return value and 1 or 0
  elseif value == nil then
    return "null"
  else
    return value
  end
end

local function convertirJsonEnBuffer(data, indices, buffer, done)
  done = done or {}
  indices = indices or {}

  if type(data) == "table" then
    if done[data] then
      return
    end
    done[data] = true

    -- pairs() ne garantit pas l'ordre sur un tableau (clés entières séquentielles) --
    -- corrigé 2026-07-19 : itération numérique explicite pour préserver l'ordre du JSON.
    local is_array = true
    for k, _ in pairs(data) do
      if type(k) ~= "number" or k < 1 or k ~= math.floor(k) or k > #data then
        is_array = false
        break
      end
    end

    local function handleEntry(key, value, nouveauxIndices)
      if type(value) == "table" then
        convertirJsonEnBuffer(value, nouveauxIndices, buffer, done)
      else
        local ligne = {}
        for _, indice in ipairs(nouveauxIndices) do
          table.insert(ligne, indice)
        end
        table.insert(ligne, sanitizeValue(value))
        table.insert(buffer, ligne)
      end
    end

    if is_array then
      for i = 1, #data do
        local nouveauxIndices = copyTable(indices)
        table.insert(nouveauxIndices, i - 1)
        handleEntry(i, data[i], nouveauxIndices)
      end
    else
      for key, value in pairs(data) do
        local nouveauxIndices = copyTable(indices)
        table.insert(nouveauxIndices, key)
        handleEntry(key, value, nouveauxIndices)
      end
    end
  else
    local ligne = copyTable(indices)
    table.insert(ligne, sanitizeValue(data))
    table.insert(buffer, ligne)
  end
end


local function loadJson(filename)
  local file = io.open(filename, "r")
  if not file then
    pd.post("Erreur : Impossible to open " .. filename)
    return nil
  end
  local jsonContent = file:read("*a")
  file:close()
  local data = lunajson.decode(jsonContent) -- Ou lunajson.decode(contenuJson)
  if not data then
    pd.post("Erreur : Impossible de décoder le fichier JSON " .. filename)
    return nil
  end
  return data
end

-- Fonction pour résoudre le chemin (absolu ou relatif)
local function resolvePath(self, filename)
  -- Convertir en string si nécessaire
  filename = tostring(filename)
  
  -- Gérer le tilde (~) pour le répertoire home
  if filename:sub(1, 1) == "~" then
    local home = os.getenv("HOME") or os.getenv("USERPROFILE")
    if home then
      filename = home .. filename:sub(2)
    end
  end
  
  -- Chemin absolu (commence par /)
  if filename:sub(1, 1) == "/" then
    return filename
  else
    -- Chemin relatif : par rapport au patch appelant (_canvaspath), pas au
    -- script pdjson.pd_lua lui-même (_loadpath) -- vérifié en conditions
    -- réelles (2026-07-18) : _loadpath pointe vers le dossier de cet
    -- external, _canvaspath vers celui du patch qui l'instancie, cohérent
    -- avec la résolution relative de midifile pour un même chemin.
    return self._canvaspath .. filename
  end
end

function pdjson:initialize(name, atoms)
  self.inlets = 1
  self.outlets = 2
  self.builder = {}
  self.builderStack = {}
  self.wsBuffer = ""
  self.json_data = nil
  self.jsonFileBuffer = {}

  -- Si un argument est fourni, charger le fichier JSON
  if atoms[1] ~= nil then
    local fname = resolvePath(self, atoms[1])
    if fname ~= nil then
      self.json_data = loadJson(fname)
      convertirJsonEnBuffer(self.json_data, {}, self.jsonFileBuffer, {})
    end
  end
  
  return true
end

function pdjson:in_1_read(atoms)
  if #atoms < 1 or type(atoms[1]) ~= "string" then
    pd.post("Error: read method expects a filename argument.")
    return
  end
  local fname = resolvePath(self, atoms[1])
  if fname ~= nil then
    self.json_data = loadJson(fname)
    self.jsonFileBuffer = {}  -- Vider le buffer avant de le remplir
    convertirJsonEnBuffer(self.json_data, {}, self.jsonFileBuffer, {})
    pd.post("JSON loaded from: " .. fname)
  end
end

-- parse <chaine JSON> -- decode une chaine deja en memoire (ex: recue par
-- websocket), sans passer par un fichier -- read() suppose toujours un
-- chemin, ce que la reception websocket n'a pas (2026-07-19).
-- outlet 2: 1 = parse reussi (json_data/jsonFileBuffer a jour, dump/get
-- utilisables), 0 = echec (JSON invalide OU juste incomplet -- lunajson ne
-- distingue pas les deux, un appelant qui reassemble un buffer reeessaiera
-- avec plus de donnees plutot que d'interpreter 0 comme une erreur fatale).
-- lunajson.decode leve une vraie erreur Lua sur echec (pas de nil-return),
-- confirme via un decode volontairement tronque : pcall est necessaire, un
-- simple "if not data" ne capture jamais l'echec (2026-07-19).
function pdjson:in_1_parse(atoms)
  if #atoms < 1 or type(atoms[1]) ~= "string" then
    pd.post("Error: parse method expects a JSON string argument.")
    self:outlet(2, "float", {0})
    return
  end
  local ok, data = pcall(lunajson.decode, atoms[1])
  if not ok or data == nil then
    self:outlet(2, "float", {0})
    return
  end
  self.json_data = data
  self.jsonFileBuffer = {}
  convertirJsonEnBuffer(self.json_data, {}, self.jsonFileBuffer, {})
  self:outlet(2, "float", {1})
end

-- wsappend <atoms...> -- reassemble les fragments livres par
-- [websocket-server] : verifie sur un vrai client websocket (2026-07-19) que
-- son flux d'octets est reconstruit en appliquant la tokenisation Pd
-- classique -- CHAQUE virgule termine un message, CHAQUE espace separe des
-- atomes DANS un message -- donc un JSON multi-champs arrive toujours
-- fragmente, jamais comme un seul atome. Chaque appel = un message recu ;
-- ses atomes sont rejoints par un espace (l'espace d'origine est perdu mais
-- l'ordre/le nombre d'atomes ne l'est pas), puis rattaches au buffer avec
-- une virgule si ce n'est pas le premier fragment. Tente un decode a chaque
-- appel ; meme signal 0/1 que parse. Sur succes, le buffer est vide pour le
-- prochain message -- wsreset permet d'abandonner un fragment partiel (ex:
-- deconnexion socket) sans attendre un decode qui n'arrivera jamais.
function pdjson:in_1_wsappend(atoms)
  local parts = {}
  for i, a in ipairs(atoms) do
    parts[i] = tostring(a)
  end
  local segment = table.concat(parts, " ")

  if self.wsBuffer == nil or self.wsBuffer == "" then
    self.wsBuffer = segment
  else
    self.wsBuffer = self.wsBuffer .. "," .. segment
  end

  local ok, data = pcall(lunajson.decode, self.wsBuffer)
  if not ok or data == nil then
    self:outlet(2, "float", {0})
    return
  end
  self.json_data = data
  self.jsonFileBuffer = {}
  convertirJsonEnBuffer(self.json_data, {}, self.jsonFileBuffer, {})
  self.wsBuffer = ""
  self:outlet(2, "float", {1})
end

function pdjson:in_1_wsreset()
  self.wsBuffer = ""
end

function pdjson:in_1_dump()
  if self.jsonFileBuffer then
    for _, line in ipairs(self.jsonFileBuffer) do
      self:outlet(1, "list", line)
    end
  end
end


-- Fonction récursive pour parcourir le JSON
function pdjson:parcourirJson(data, args, index, indices)
  indices = indices or {}
  local output = indices or {}
  if index > #args then
    if type(data) == "number" then
      table.insert(output, data)
      self:outlet(1, "list", output)
    elseif type(data) == "string" then
      table.insert(output, data)
      self:outlet(1, "list", output)
    elseif type(data) == "table" then
      if next(data) == nil then
        self:outlet(1, "bang")
      else
        for key, value in pairs(data) do
          local nouveauxIndices = copyTable(indices)
          if type(key) == "number" then
            table.insert(nouveauxIndices, key - 1)
          else
            table.insert(nouveauxIndices, key)
          end
          self:parcourirJson(value, {}, 1, nouveauxIndices)
        end
      end
    else
      pd.post("Type de données non géré : " .. type(data))
    end
    return
  end

  local arg = args[index]
  if type(arg) == "string" then
    if type(data) == "table" and data[arg] then
      table.insert(indices, arg)
      self:parcourirJson(data[arg], args, index + 1, indices)
    else
      pd.post("Clé '" .. arg .. "' non trouvée.")
    end
  elseif type(arg) == "number" then
    if type(data) == "table" and type(data[arg + 1]) ~= "nil" then
      table.insert(indices, arg)
      self:parcourirJson(data[arg + 1], args, index + 1, indices)
    else
      pd.post("Indice " .. arg .. " non trouvé.")
    end
  else
    pd.post("Argument invalide : " .. arg)
  end
end


-- Fonction principale pour gérer les arguments
function pdjson:in_1_get(atoms)
  self:parcourirJson(self.json_data, atoms, 1, {})
end


-- Fonction récursive pour parcourir le JSON et obtenir les clés
function pdjson:parcourirCles(data, indices, output)
  indices = indices or {}
  output = output or {}

  if type(data) == "table" then
    local index = 0
    for key, value in pairs(data) do
      local nouveauxIndices = copyTable(indices)
      if type(key) == "number" then
        table.insert(nouveauxIndices, index)
        index = index + 1
      else
        table.insert(nouveauxIndices, key)
      end
      table.insert(output, nouveauxIndices) -- Ajoute la liste de clés au tableau de sortie
      self:parcourirCles(value, nouveauxIndices, output)
    end
  end
end

-- deepcopy function (include this if you don't have it already)
local function deepcopy(orig)
  local orig_type = type(orig)
  local copy
  if orig_type == 'table' then
      copy = {}
      for orig_key, orig_value in next, orig, nil do
          copy[deepcopy(orig_key)] = deepcopy(orig_value)
      end
      setmetatable(copy, deepcopy(getmetatable(orig)))
  else -- number, string, boolean, etc
      copy = orig
  end
  return copy
end

function pdjson:in_1_set(atoms)
  local new_json_data = self.json_data and deepcopy(self.json_data) or {}
  local current_table = new_json_data
  if #atoms < 2 then
      pd.post("Error: Update requires at least a key and a value.")
      return
  end
  for j = 1, #atoms - 1 do
      local key = atoms[j]
      local num_key = tonumber(key)
      if num_key then
          key = num_key + 1
      end
      if type(current_table) ~= "table" then
          pd.post("Error: Path element is not a table: " .. tostring(atoms[j]))
          return
      end
      if current_table[key] == nil and j < #atoms - 1 then -- Create nested tables only when needed
          current_table[key] = {}
      elseif j < #atoms - 1 and type(current_table[key]) ~= "table" then
          pd.post("Error: Path element is not a table: " .. tostring(atoms[j]))
          return
      end
      if j < #atoms - 1 then -- Only traverse if not the last key
          current_table = current_table[key]
      end
  end
  local last_key = atoms[#atoms - 1]
  local num_last_key = tonumber(last_key)
  if num_last_key then
      last_key = num_last_key + 1
  end
  local value = atoms[#atoms]
  if value == "null" then value = nil else value = tonumber(value) or value end
  if type(current_table) ~= "table" then -- Final check before assignment
      pd.post("Error: Cannot assign to non-table at: " .. tostring(atoms[#atoms - 1]))
      return
  end
  current_table[last_key] = value
  self.json_data = new_json_data
  convertirJsonEnBuffer(self.json_data, {}, self.jsonFileBuffer, {})
end

-- ===== Construction JSON incrémentale (add/array/push/pop/clear) =====
-- Complète get/set (qui exigent un JSON déjà chargé dans json_data) par un
-- constructeur à part : self.builder est une table Lua mutable, self.builderStack
-- le curseur de sous-objet courant (push/pop). Voir PATCH_REBUILD.md §1 —
-- portée volontairement réduite par rapport à PuRestJson (une seule table
-- mutable avec curseur, pas d'instanciation de plusieurs objets pdjson).

local function builderCursor(self)
  return self.builderStack[#self.builderStack] or self.builder
end

function pdjson:in_1_add(atoms)
  if #atoms ~= 2 then
    pd.post("Error: add expects exactly a key and a value.")
    return
  end
  builderCursor(self)[tostring(atoms[1])] = atoms[2]
end

-- Comme set, mais sur le builder au lieu de json_data. Ecrit <valeur> au bout
-- d'un <chemin...> (cles string, ou index numeriques 0-based comme partout
-- ailleurs dans cet external), en creant les tables intermediaires au besoin.
-- Raison d'etre : <cle...> <valeur> est EXACTEMENT la forme que dump sort, donc
-- setB permet de reinjecter tel quel le dump d'un autre pdjson dans le builder,
-- sans avoir a le retraduire en sequence push/add cote patch -- indispensable
-- pour agreger N fichiers de scenes en un seul payload (pedalier: scenesList).
-- Contrairement a set, PAS de deepcopy : appele une fois par ligne de dump, soit
-- des centaines de fois par payload, copier tout l'arbre a chaque appel serait
-- ruineux. Le chemin part du curseur courant (comme add/array), donc de la
-- racine du builder tant qu'aucun push n'est actif.
function pdjson:in_1_setB(atoms)
  if #atoms < 2 then
    pd.post("Error: setB requires at least a key and a value.")
    return
  end
  local current = builderCursor(self)
  for j = 1, #atoms - 2 do
    local key = atoms[j]
    local num_key = tonumber(key)
    if num_key then key = num_key + 1 end
    if type(current[key]) ~= "table" then
      current[key] = {}
    end
    current = current[key]
  end
  local last_key = atoms[#atoms - 1]
  local num_last = tonumber(last_key)
  if num_last then last_key = num_last + 1 end
  local value = atoms[#atoms]
  if value == "null" then
    -- pas de nil en JSON cote Pd : la convention est d'effacer la cle, ce que
    -- dump/get traitent deja comme une absence (une cle nulle n'apparait pas
    -- dans un dump). Ecrire la chaine "null" ferait passer un vide pour un nom.
    current[last_key] = nil
  else
    current[last_key] = tonumber(value) or value
  end
end

function pdjson:in_1_array(atoms)
  if #atoms ~= 2 then
    pd.post("Error: array expects exactly a key and a value.")
    return
  end
  local key = tostring(atoms[1])
  local cursor = builderCursor(self)
  if type(cursor[key]) ~= "table" then
    cursor[key] = {}
  end
  table.insert(cursor[key], atoms[2])
end

function pdjson:in_1_push(atoms)
  if #atoms ~= 1 then
    pd.post("Error: push expects a single key argument.")
    return
  end
  local key = tostring(atoms[1])
  local cursor = builderCursor(self)
  if type(cursor[key]) ~= "table" then
    cursor[key] = {}
  end
  table.insert(self.builderStack, cursor[key])
end

-- Comme push, mais pour un tableau d'objets distincts (ex. loops.states[]) :
-- ajoute un nouvel élément (table vide) au tableau à <key> et descend le
-- curseur dedans -- pop symétrique existant referme l'élément, prêt pour le
-- suivant. push seul ne couvre pas ce cas : il redescend dans LE MÊME objet
-- à chaque appel, il ne crée pas un nouvel élément de tableau.
function pdjson:in_1_pushArray(atoms)
  if #atoms ~= 1 then
    pd.post("Error: pushArray expects a single key argument.")
    return
  end
  local key = tostring(atoms[1])
  local cursor = builderCursor(self)
  if type(cursor[key]) ~= "table" then
    cursor[key] = {}
  end
  local element = {}
  table.insert(cursor[key], element)
  table.insert(self.builderStack, element)
end

function pdjson:in_1_pop()
  if #self.builderStack == 0 then
    pd.post("Error: pop with no matching push.")
    return
  end
  table.remove(self.builderStack)
end

-- Nommé clearBuilder (pas "clear") : vérifié en conditions réelles dans Pd
-- (2026-07-17) -- un message "clear" nu n'atteint jamais in_1_clear, pdlua
-- ou Pd core semble intercepter ce nom avant la résolution de méthode
-- personnalisée. "clearBuilder" n'a pas ce conflit.
function pdjson:in_1_clearBuilder()
  self.builder = {}
  self.builderStack = {}
end

local function table_to_json(tbl, indent_level)
  local indent = string.rep("  ", indent_level or 0)
  local result = ""

  if next(tbl) == nil then
      return "{}"
  end

  local is_array = true
  for k, _ in pairs(tbl) do
      if type(k) ~= "number" or k < 1 or k ~= math.floor(k) or k > #tbl then
          is_array = false
          break
      end
  end

  if is_array then
      result = "[\n"
      for i = 1, #tbl do
          if i > 1 then
              result = result .. ",\n"
          end
          result = result .. indent .. "  "  -- Indent array elements
          local v = tbl[i]
          if type(v) == "table" then
              result = result .. table_to_json(v, (indent_level or 0) + 1) -- Recursive call with increased indent
          elseif type(v) == "string" then
              result = result .. "\"" .. v:gsub("\\", "\\\\"):gsub("\"", "\\\""):gsub("\n", "\\n"):gsub("\r", "\\r"):gsub("\t", "\\t") .. "\""
          else
              result = result .. tostring(v)
          end
      end
      result = result .. "\n" .. indent .. "]"
  else
      result = "{\n"
      local first = true
      for k, v in pairs(tbl) do
          if not first then
              result = result .. ",\n"
          end
          first = false
          result = result .. indent .. "  "  -- Indent key-value pairs

          if type(k) == "string" then
              result = result .. "\"" .. k:gsub("\\", "\\\\"):gsub("\"", "\\\""):gsub("\n", "\\n"):gsub("\r", "\\r"):gsub("\t", "\\t") .. "\":"
          else
              result = result .. k - 1 .. ":"
          end

          if type(v) == "table" then
              result = result .. table_to_json(v, (indent_level or 0) + 1) -- Recursive call with increased indent
          elseif type(v) == "string" then
              result = result .. "\"" .. v:gsub("\\", "\\\\"):gsub("\"", "\\\""):gsub("\n", "\\n"):gsub("\r", "\\r"):gsub("\t", "\\t") .. "\""
          else
              result = result .. tostring(v)
          end
      end
      result = result .. "\n" .. indent .. "}"
  end

  return result
end

-- In your save function, call table_to_json with an initial indent level of 0:
-- file:write(table_to_json(json_data, 0))

function pdjson:in_1_write(atoms)
  if #atoms ~= 1 or type(atoms[1]) ~= "string" then
    pd.post("Error: write method expects a single filename argument.")
    return
  end

  local filename = resolvePath(self, atoms[1])
  local file, err = io.open(filename, "w")
  if not file then
    pd.post("Error: Could not open file for writing: " .. err)
    return
  end

  local json_string = table_to_json(self.json_data)
  file:write(json_string)
  file:close()

  pd.post("JSON data saved to: " .. filename)
end

-- Sérialise self.builder (pas json_data) en une seule chaîne, envoyée comme
-- un unique atome symbole sur l'outlet — payload prêt pour sendBinaryMessage
-- côté QML sans repasser par un fichier.
function pdjson:in_1_build()
  self:outlet(1, "symbol", { table_to_json(self.builder) })
end

-- Persiste self.builder sur disque, symétrique de in_1_write (qui persiste
-- json_data). Utilisé par les patchs qui construisent un objet via
-- add/array/push/pop puis veulent le sauver sans repasser par set/write.
function pdjson:in_1_writeBuilder(atoms)
  if #atoms ~= 1 or type(atoms[1]) ~= "string" then
    pd.post("Error: writeBuilder method expects a single filename argument.")
    return
  end

  local filename = resolvePath(self, atoms[1])
  local file, err = io.open(filename, "w")
  if not file then
    pd.post("Error: Could not open file for writing: " .. err)
    return
  end

  file:write(table_to_json(self.builder))
  file:close()

  pd.post("Builder JSON saved to: " .. filename)
end

function pdjson:in_1_dumpBinary()
  if not self.json_data then
    pd.post("Error: No JSON data loaded")
    return
  end

  local message = {
    type = "CONFIG_FULL",
    config = self.json_data
  }
  
  local jsonString = lunajson.encode(message)
  local jsonSize = #jsonString
  local chunkSize = 4096
  
  local function int32ToBytes(num)
    return {
      num % 256,
      math.floor(num / 256) % 256,
      math.floor(num / 65536) % 256,
      math.floor(num / 16777216) % 256
    }
  end
  
  -- Découper en chunks et envoyer
  for i = 1, jsonSize, chunkSize do
    local chunk = {"BINARY"}
    
    -- Ajouter jsonSize (4 bytes)
    for _, b in ipairs(int32ToBytes(jsonSize)) do
      table.insert(chunk, b)
    end
    
    -- Ajouter la position actuelle dans le buffer (4 bytes)
    for _, b in ipairs(int32ToBytes(i - 1)) do
      table.insert(chunk, b)
    end
    
    -- Ajouter les données
    for j = i, math.min(i + chunkSize - 1, jsonSize) do
      table.insert(chunk, string.byte(jsonString, j))
    end
    
    self:outlet(1, "list", chunk)
  end
  
  pd.post("Binary dump sent: " .. jsonSize .. " bytes in chunks of " .. chunkSize)
end
