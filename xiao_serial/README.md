# xiao_serial

Un external pour Pure Data qui permet la communication série USB bidirectionnelle avec un XIAO ESP32S3.

## Description

`xiao_serial` ouvre un port série USB, lit les données reçues de l'ESP32S3 dans un thread séparé, et permet d'envoyer des bytes vers l'ESP32S3. Les données sont transmises sous forme de bytes bruts (valeurs 0-255).

## Utilisation

L'objet a:
- **1 inlet**: reçoit une liste de bytes (floats 0-255) à envoyer vers l'ESP32S3
- **1 outlet**: sort une liste de bytes (floats 0-255) reçus de l'ESP32S3

## Méthodes

### `open <port> [baudrate]`

Ouvre le port série USB.

**Arguments:**
- `port`: Chemin du port série (ex: `/dev/ttyUSB0`, `/dev/ttyACM0`)
- `baudrate`: Vitesse de transmission en bauds (optionnel, défaut: 115200)

**Exemple:**
```
[msg open /dev/ttyACM0 115200(
|
[xiao_serial]
```

### `close`

Ferme le port série.

**Exemple:**
```
[msg close(
|
[xiao_serial]
```

### `interval <ms>`

Définit l'intervalle de polling en millisecondes (défaut: 10ms). Plus l'intervalle est court, plus la latence est faible, mais plus la charge CPU est élevée.

**Exemple:**
```
[msg interval 5(
|
[xiao_serial]
```

### `list <bytes...>`

Envoie une liste de bytes vers l'ESP32S3. Les valeurs sont automatiquement limitées à la plage 0-255.

**Exemple:**
```
[list 65 66 67(
|
[xiao_serial]
```

### `scan`

Liste tous les ports série disponibles sur le système. Les ports sont envoyés sous forme de liste de symboles sur l'outlet.

**Exemple:**
```
[msg scan(
|
[xiao_serial] -> [print ports]
```

Cette méthode recherche les ports dans :
- `/dev/cu.*` (macOS callout ports)
- `/dev/tty.usb*` (macOS USB ports)
- `/dev/ttyUSB*` (Linux USB serial)
- `/dev/ttyACM*` (Linux ACM/CDC ports)

## Exemple complet

```
# Patch PureData pour communiquer avec ESP32S3

# Ouvrir le port
[msg open /dev/ttyACM0 115200(
|
[xiao_serial]

# Afficher les bytes reçus
[xiao_serial] -> [unpack f f f f]
                |    |    |    |
            print0 print1 print2 print3

# Envoyer des bytes
[list 72 101 108 108 111(  # "Hello" en ASCII
|
[xiao_serial]

# Fermer le port
[msg close(
|
[xiao_serial]
```

## Configuration du port série

L'external configure automatiquement le port série avec les paramètres suivants:
- **Format**: 8 bits, pas de parité, 1 bit de stop (8N1)
- **Mode**: Raw (pas de traitement de caractères)
- **Timeout**: 0.1 seconde
- **Baudrates supportés**: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000, 921600, 1000000

## Permissions

Sur Linux, l'accès au port série nécessite des permissions appropriées. Si vous obtenez une erreur "Permission denied", ajoutez votre utilisateur au groupe `dialout`:

```bash
sudo usermod -a -G dialout $USER
```

Puis déconnectez-vous et reconnectez-vous pour que les changements prennent effet.

Vous pouvez vérifier les permissions avec:
```bash
ls -l /dev/ttyUSB0
# ou
ls -l /dev/ttyACM0
```

## Détection du port série

Pour trouver le port série de votre XIAO ESP32S3:

```bash
# Lister les ports série disponibles
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null

# Avant et après branchement de l'ESP32S3
dmesg | tail
```

Le port apparaît généralement sous `/dev/ttyACM0` ou `/dev/ttyUSB0` selon le pilote USB utilisé.

## Architecture technique

- **Thread de lecture**: Un thread séparé lit les données du port série de manière non-bloquante
- **Buffer circulaire**: Les données reçues sont stockées dans un buffer circulaire thread-safe
- **Clock PureData**: Un clock lit périodiquement le buffer et envoie les données via l'outlet
- **Mutex**: Protection thread-safe pour l'accès au buffer et à l'écriture

## Compilation

```bash
cd xiao_serial
make clean
make
```

L'external est compilé avec support termios sur Linux et macOS, et en mode stub sur les autres plateformes.

## Intégration avec d'autres externals

`xiao_serial` peut être utilisé avec d'autres externals critapec:

- **Parser des données**: Utiliser `bytes2int` ou `bytes2float` pour convertir les bytes en valeurs numériques
- **Format CB4Tech**: Si l'ESP32S3 envoie un format similaire, utiliser `cb4tech_parse` pour parser les données

**Exemple:**
```
[xiao_serial] -> [bytes2int] -> [print value]
```

## Dépannage

### Le port ne s'ouvre pas
- Vérifier que le port existe: `ls -l /dev/ttyACM0`
- Vérifier les permissions (voir section Permissions)
- Vérifier qu'aucun autre processus n'utilise le port: `lsof /dev/ttyACM0`

### Aucune donnée reçue
- Vérifier que l'ESP32S3 envoie bien des données (test avec `minicom` ou `screen`)
- Vérifier que le baudrate correspond (115200 par défaut)
- Vérifier la connexion USB

### Erreurs de compilation
- Vérifier que `pthread` est installé: `sudo apt-get install libc6-dev` (Linux)
- Vérifier que le Makefile.pdlibbuilder est présent dans `pd-lib-builder/`

## Licence

GPL-2.0+

## Auteur

Patrice Colet, 2025
