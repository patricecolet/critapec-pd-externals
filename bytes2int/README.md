# bytes2int

Un external pour Pure Data qui convertit 4 bytes en entier 32 bits signé, avec sélection de l'endianness (little par défaut, big optionnel).

## Description

`bytes2int` prend une liste de bytes (0-255) et :
- convertit les 4 premiers bytes en `int32` selon l'endianness choisie,
- sort le résultat sur l'outlet gauche,
- et sort sur l'outlet droit les bytes supplémentaires ou invalides.

Endianness:
- Par défaut: little-endian
- À la création: `[bytes2int big]` pour big-endian
- Dynamiquement: message `endian little` ou `endian big`

## Comportement
- Moins de 4 bytes: complété par des `0` à droite
- Plus de 4 bytes: les 4 premiers convertis, le reste sur l'outlet droit
- Byte invalide (<0 ou >255): rien sur l'outlet gauche; tout sur l'outlet droit

## Utilisation

```
[1 0 0 0(
|
[bytes2int]
|
[print int]
```

```
[0 0 0 1(
|
[bytes2int big]
|
[print int-big]
```

Changer l'endianness dynamiquement:
```
[endian big(
|
[bytes2int]
```

## Compilation

```bash
make
```

## Licence

GPL-2.0+



