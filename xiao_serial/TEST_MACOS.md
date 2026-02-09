# Test de xiao_serial sur macOS

## Installation pour test

Sur macOS, l'external est compilé en mode stub (sans support termios), donc il ne pourra pas vraiment communiquer avec un port série. Cependant, on peut tester que :
- L'external se charge correctement
- Les méthodes répondent
- La structure générale fonctionne

### Option 1 : Lien symbolique (recommandé)

```bash
# Créer le dossier critapec s'il n'existe pas
mkdir -p ~/Library/Pd/critapec

# Créer un lien symbolique vers l'external
ln -s /Users/patricecolet/repo/pd-externals/critapec/xiao_serial ~/Library/Pd/critapec/xiao_serial
```

### Option 2 : Copie directe

```bash
# Créer le dossier critapec s'il n'existe pas
mkdir -p ~/Library/Pd/critapec

# Copier l'external
cp -r /Users/patricecolet/repo/pd-externals/critapec/xiao_serial ~/Library/Pd/critapec/
```

## Test dans PureData

1. **Ouvrir PureData**

2. **Ouvrir le patch de test** :
   - Ouvrir le fichier `test_xiao_serial.pd` depuis le dossier xiao_serial
   - Ou créer un nouveau patch et ajouter `[xiao_serial]`

3. **Vérifier le chargement** :
   - Dans la console PureData, vous devriez voir :
     ```
     xiao_serial v0.1 - Communication série USB avec XIAO ESP32S3
       Mode stub (termios non disponible)
     ```

4. **Tester les méthodes** :
   - Cliquer sur `[msg open /dev/ttyUSB0 115200]` :
     - Vous verrez un warning : "xiao_serial: compilé sans support termios"
     - C'est normal en mode stub sur macOS
   
   - Cliquer sur `[msg close]` :
     - Devrait fonctionner sans erreur
   
   - Envoyer `[list 72 101 108 108 111]` :
     - Vous verrez un warning similaire
     - C'est normal, l'écriture ne fonctionne pas en mode stub

5. **Tester l'intervalle** :
   - Cliquer sur `[msg interval 10]`
   - Devrait fonctionner sans erreur

## Résultats attendus

En mode stub sur macOS :
- ✅ L'external se charge correctement
- ✅ Les méthodes répondent (même si elles affichent des warnings)
- ✅ La structure générale fonctionne
- ❌ La communication série ne fonctionne pas (normal, nécessite Linux)

## Test réel avec port série

Pour un test réel avec communication série, il faut :
1. Compiler sur Linux (Raspberry Pi)
2. Avoir un XIAO ESP32S3 connecté
3. Configurer les permissions (groupe dialout)

## Dépannage

### L'external ne se charge pas

Vérifier que le fichier est au bon endroit :
```bash
ls -la ~/Library/Pd/critapec/xiao_serial/xiao_serial.pd_darwin
```

Vérifier les chemins de recherche de PureData :
- Menu PureData > Preferences > Path
- Ajouter `~/Library/Pd` si nécessaire

### Erreurs de compilation

Si vous recompilez sur macOS :
```bash
cd /Users/patricecolet/repo/pd-externals/critapec/xiao_serial
make clean
make
```
