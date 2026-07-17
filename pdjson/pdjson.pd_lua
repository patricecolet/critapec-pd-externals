local lunajson = require 'lunajson'

local pdjson = pd.Class:new():register("pdjson")

local json_data = nil
local jsonFileBuffer = {}

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

    local index = 0
    for key, value in pairs(data) do
      local nouveauxIndices = copyTable(indices)

      if type(key) == "number" then
        table.insert(nouveauxIndices, index)
        index = index + 1
      else
        table.insert(nouveauxIndices, key)
      end

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
    -- Chemin relatif : ajouter le loadpath
    return self._loadpath .. filename
  end
end

function pdjson:initialize(name, atoms)
  self.inlets = 1
  self.outlets = 1
  self.builder = {}
  self.builderStack = {}

  -- Si un argument est fourni, charger le fichier JSON
  if atoms[1] ~= nil then
    local fname = resolvePath(self, atoms[1])
    if fname ~= nil then
      json_data = loadJson(fname)
      convertirJsonEnBuffer(json_data, {}, jsonFileBuffer, {})
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
    json_data = loadJson(fname)
    jsonFileBuffer = {}  -- Vider le buffer avant de le remplir
    convertirJsonEnBuffer(json_data, {}, jsonFileBuffer, {})
    pd.post("JSON loaded from: " .. fname)
  end
end
  
function pdjson:in_1_dump()
  if jsonFileBuffer then
    for _, line in ipairs(jsonFileBuffer) do
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
  self:parcourirJson(json_data, atoms, 1, {})
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
  local new_json_data = json_data and deepcopy(json_data) or {}
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
  local value = tonumber(atoms[#atoms]) or atoms[#atoms]
  if type(current_table) ~= "table" then -- Final check before assignment
      pd.post("Error: Cannot assign to non-table at: " .. tostring(atoms[#atoms - 1]))
      return
  end
  current_table[last_key] = value
  json_data = new_json_data
  convertirJsonEnBuffer(json_data, {}, jsonFileBuffer, {})
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

  local json_string = table_to_json(json_data)
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
  if not json_data then
    pd.post("Error: No JSON data loaded")
    return
  end
  
  local message = {
    type = "CONFIG_FULL",
    config = json_data
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
