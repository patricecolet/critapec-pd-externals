# cb4tech_parse

Un external pour Pure Data qui parse les données des capteurs CB4Tech.

## Description

`cb4tech_parse` reçoit une liste de bytes représentant la structure `sensors_values_t` et sort les valeurs groupées par type de contrôleur sur un seul outlet.

## Structure de données

La structure parsée contient 30 bytes organisés comme suit :

```c
typedef struct __attribute__((packed)) {
    uint32_t absolute_encoder;
    int16_t encoder;
    int16_t velocity;
    uint16_t absolute_force1;
    uint16_t absolute_force2;
    uint16_t continuous_force1;
    uint16_t continuous_force2;
    uint8_t drum1;           // 0-127
    uint8_t drum2;           // 0-127
    int8_t joystick_x;
    int8_t joystick_y;
    int8_t joystick_z;
    uint8_t joystick_button;
    uint8_t selecter;
    int16_t ext_adc0;
    int16_t ext_adc1;
    uint8_t ext_button1;
    uint8_t ext_button2;
} sensors_values_t;
```

## Utilisation

L'objet a:
- **1 inlet**: reçoit une liste de 30 bytes
- **1 outlet**: sort des messages groupés par type de contrôleur

## Messages de sortie

Les données sont envoyées sous forme de messages avec un symbole suivi d'une liste de floats :

- `encoder <encoder> <velocity> <absolute_encoder>` - 3 valeurs
- `adc <adc0> <adc1>` - 2 valeurs
- `selector <valeur>` - 1 valeur
- `joystick <x> <y> <z> <bouton>` - 4 valeurs
- `drum <drum1> <drum2> <continuous_force1> <continuous_force2> <absolute_force1> <absolute_force2>` - 6 valeurs
- `button <button1> <button2>` - 2 valeurs

### Exemple

```
[liste de 30 bytes(
|
[cb4tech_parse]
|
[route encoder adc selector joystick drum button]
|        |        |        |        |        |
encoder  adc   selector joystick  drum   button
```

Envoyez un bang pour afficher la taille de données attendue.

## Compilation

```bash
make
```

## Installation

Copiez le fichier compilé dans un dossier accessible par Pure Data.

## Licence

GPL-2.0+

## Auteur

Patrice Colet, 2025
