# encoder2note

Un external pour Pure Data qui convertit le nombre de tours d'un encodeur rotatif en notes musicales avec demi-tons et centièmes de ton, selon la position d'un levier de vitesse.

## Description

`encoder2note` est conçu pour des applications musicales utilisant un volant avec encodeur rotatif et un levier de vitesse. Il convertit le nombre de tours du volant en notes musicales exprimées en demi-tons avec décimales (partie décimale = centièmes de ton).

L'objet utilise un **accumulateur** pour éviter les sauts de notes lors du changement de vitesse et un **ambitus** pour limiter l'étendue des notes entre un minimum et un maximum. Cela permet de changer de vitesse en temps réel sans discontinuité dans la sortie musicale.

## Caractéristiques

- **Calcul par delta** : Détecte le mouvement du volant et accumule les changements
- **Vitesse variable** : De 1 à 48 demi-tons par tour complet (valeurs continues, pas de hardcoding)
- **Pas de sauts** : L'accumulateur garantit une continuité lors du changement de vitesse
- **Ambitus configurable** : Limite les notes entre un minimum et un maximum
- **Sortie simple** : Demi-tons avec décimales (partie décimale = centièmes)

## Utilisation

### Inlets

- **Inlet gauche** : Reçoit le nombre de tours (ex: 1 = un tour, 2.5 = deux tours et demi)
- **Inlet droit** : Reçoit la vitesse en demi-tons/tour (de 1 à 48)

### Outlets

- **Outlet unique** : Demi-tons avec décimales (ex: 12.50 = 12 demi-tons + 50 centièmes)

### Création de l'objet

```
[encoder2note]                  # Vitesse = 12, ambitus = -48 à +48
[encoder2note 24]               # Vitesse = 24, ambitus par défaut
[encoder2note 6]                # Vitesse = 6 (demi-octave/tour)
[encoder2note 12 0 24]          # Vitesse = 12, ambitus de 0 à 24 demi-tons
[encoder2note 18.5 -12 36]      # Vitesse = 18.5, ambitus personnalisé
```

### Messages

- `reset` : Remet l'accumulateur à 0
- `set N` : Définit manuellement l'accumulateur à N demi-tons
- `min N` : Définit la note minimum (ambitus)
- `max N` : Définit la note maximum (ambitus)
- `gear N` : Définit la vitesse (N entre 1 et 48, avec validation)

## Exemple de comportement

### Avec accumulateur

1. Tournez de 1 tour avec vitesse 12 → Accumulateur = 12.0 → Sortie: 12.00
2. Tournez de 0.5 tour → Accumulateur = 18.0 → Sortie: 18.00
3. **Changez vitesse à 24** → Accumulateur reste à 18.0 → **Pas de saut!**
4. Tournez de 0.5 tour → Accumulateur = 30.0 → Sortie: 30.00 (0.5 × 24 = 12)

L'avantage : vous pouvez changer de vitesse en temps réel sans créer de discontinuité dans la mélodie.

### Avec ambitus

Avec `[encoder2note 12 0 24]` :

1. Tournez jusqu'à atteindre 24 demi-tons → Sortie: 24.00
2. Continuez à tourner → **Sortie reste bloquée à 24.00** (max atteint)
3. Tournez en arrière → La sortie redescend
4. Atteignez 0 demi-ton → Sortie: 0.00
5. Continuez en arrière → **Sortie reste bloquée à 0.00** (min atteint)

L'ambitus empêche de sortir des limites définies, idéal pour restreindre à une tessiture spécifique.

## Exemples de vitesses

| Vitesse | Description |
|---------|-------------|
| 1       | Très lent (variations microtonales, 1 demi-ton/tour) |
| 6       | Demi-octave par tour |
| 12      | Une octave par tour |
| 18      | Octave et demie par tour |
| 24      | Deux octaves par tour |
| 36      | Trois octaves par tour |
| 48      | Quatre octaves par tour (maximum) |

**Note :** Vous pouvez utiliser n'importe quelle valeur entre 1 et 48, même avec décimales (ex: 15.5).

## Patch d'exemple

```
[hsl 200 15 0 1]  # Slider pour simuler l'encodeur
|
[encoder2note 12]
|          |
[demi-tons][centièmes]
```

## Compilation

```bash
make
```

Le Makefile utilise pd-lib-builder pour une compilation multiplateforme.

## Installation

Copiez le fichier compilé (`encoder2note.pd_darwin`, `encoder2note.pd_linux`, etc.) dans un dossier accessible par Pure Data, ou utilisez :

```bash
make install
```

## Applications

Cet external est particulièrement adapté pour :

- Interfaces de contrôle motorisées (volants, encodeurs)
- Synthèse granulaire avec contrôle continu
- Instruments électroniques avec variations microtonales
- Contrôleurs MIDI avec haute résolution

## Licence

GPL-2.0+

## Auteur

Patrice Colet, 2025

## Voir aussi

- `mtof` - Convertit MIDI en fréquence
- `ftom` - Convertit fréquence en MIDI

