# bytes2float

Un external pour Pure Data qui convertit 4 bytes au format little-endian en float32.

## Description

`bytes2float` prend une liste de bytes en entrée (0-255) et sort un float représentant ces bytes au format IEEE 754 little-endian. C'est l'opération inverse de `float2byte`.

L'objet a deux outlets :
- **Outlet 0 (gauche)** : sort le float converti
- **Outlet 1 (droite)** : sort les bytes supplémentaires ou invalides

## Comportement

- **Si moins de 4 bytes** : complète avec 0 pour avoir 4 bytes, puis convertit en float
- **Si plus de 4 bytes** : prend les 4 premiers bytes pour la conversion, sort le reste sur l'outlet 1
- **Si bytes invalides** (< 0 ou > 255) : ne sort rien sur l'outlet 0, sort tous les bytes invalides sur l'outlet 1

## Utilisation

L'objet a:
- **1 inlet**: reçoit une liste de bytes (0-255)
- **2 outlets**: 
  - Outlet 0: float converti
  - Outlet 1: bytes supplémentaires ou invalides

### Exemple

```
[pack f f f f 63 144 68 66(
|
[bytes2float]
|       |
float   bytes supplémentaires
```

## Compilation

```bash
make
```

## Installation

Copiez le fichier compilé (bytes2float.pd_darwin, bytes2float.pd_linux, etc.) dans un dossier accessible par Pure Data.

## Licence

GPL-2.0+

## Auteur

Patrice Colet, 2025

