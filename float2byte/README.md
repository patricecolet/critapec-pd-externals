# float2byte

Un external pour Pure Data qui convertit un float32 en bytes au format little-endian.

## Description

`float2byte` prend un nombre float en entrée et sort une liste de 4 bytes représentant ce float au format IEEE 754 little-endian.

## Utilisation

L'objet a:
- **1 inlet**: reçoit des floats
- **1 outlet**: sort une liste de 4 bytes (valeurs 0-255)

### Exemple

```
[3.14159(
|
[float2byte]
|
[unpack f f f f]
|    |    |    |
byte0 byte1 byte2 byte3
```

## Compilation

```bash
make
```

## Installation

Copiez le fichier compilé (float2byte.pd_darwin, float2byte.pd_linux, etc.) dans un dossier accessible par Pure Data.

## Licence

GPL-2.0+

## Auteur

Patrice Colet, 2025

