# uint32tobytes

Un external pour Pure Data qui convertit un uint32_t en bytes au format little-endian.

## Description

`uint32tobytes` prend un nombre float en entrée (converti en uint32_t) et sort une liste de 4 bytes représentant ce uint32_t au format little-endian.

Les valeurs négatives sont traitées comme 0, et les valeurs supérieures à 4294967295 (UINT32_MAX) sont saturées à cette valeur maximale.

## Utilisation

L'objet a:
- **1 inlet**: reçoit des floats (convertis en uint32_t)
- **1 outlet**: sort une liste de 4 bytes (valeurs 0-255)

### Exemple

```
[4294967295(
|
[uint32tobytes]
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

Copiez le fichier compilé (uint32tobytes.pd_darwin, uint32tobytes.pd_linux, etc.) dans un dossier accessible par Pure Data.

## Licence

GPL-2.0+

## Auteur

Patrice Colet, 2025

