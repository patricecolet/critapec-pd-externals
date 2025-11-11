# encoder2note

Un external pour Pure Data qui convertit la position d'un encodeur rotatif (0-1) en notes musicales avec demi-tons et centièmes de ton, selon la position d'un levier de vitesse.

## Description

`encoder2note` est conçu pour des applications musicales utilisant un volant avec encodeur rotatif et un levier de vitesse. Il reçoit la position de l'encodeur (normalisée de 0 à 1 pour un tour complet) et **compte automatiquement les tours** pour générer des notes musicales exprimées en demi-tons avec décimales (partie décimale = centièmes de ton).

L'objet utilise un **accumulateur** pour éviter les sauts de notes lors du changement de vitesse, **détecte automatiquement les passages de tours** (wrap-around), et utilise un **ambitus** pour limiter l'étendue des notes entre un minimum et un maximum. Cela permet de changer de vitesse en temps réel sans discontinuité dans la sortie musicale.

## Caractéristiques

- **Comptage automatique des tours** : Reçoit position 0-1, détecte et compte les tours automatiquement
- **Détection du wrap-around** : Détecte les passages de 1→0 (tour avant) et 0→1 (tour arrière)
- **Calcul par delta** : Détecte le mouvement du volant et accumule les changements
- **Vitesse variable** : De 1 à 48 demi-tons par tour complet (valeurs continues, pas de hardcoding)
- **Pas de sauts** : L'accumulateur garantit une continuité lors du changement de vitesse
- **Ambitus configurable** : Limite les notes entre un minimum et un maximum
- **Sortie simple** : Demi-tons avec décimales (partie décimale = centièmes)

## Utilisation

### Inlets

- **Inlet gauche** : Reçoit la position de l'encodeur (0 à 1 pour un tour, les tours sont comptés automatiquement)
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

### Avec comptage automatique des tours

1. Encodeur envoie 0 → 0.25 → 0.5 → 0.75 → 1.0 (un tour complet, vitesse 12)
   - À 0.5 (demi-tour) : Sortie = 6.0
   - À 1.0 (tour complet) : Sortie = 12.0
2. Encodeur continue : 1.0 → 0 → 0.25 (détecte le wrap-around, c'est un nouveau tour)
   - À 0.25 du 2ème tour : Sortie = 15.0
3. **Changez vitesse à 24** → Accumulateur reste à 15.0 → **Pas de saut!**
4. Continuez à tourner de 0.25 tour → Sortie = 21.0 (0.25 × 24 = 6, ajouté à 15)

L'avantage : vous pouvez faire autant de tours que vous voulez, dans les deux sens, et changer de vitesse en temps réel sans créer de discontinuité dans la mélodie.

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
[hsl 200 15 0 1 0 0]  # Slider 0-1 pour simuler l'encodeur
|
[encoder2note 12]
|
[demi-tons + centièmes]
```

**Note importante** : L'encodeur doit envoyer une valeur normalisée entre 0 et 1 pour un tour complet. L'external détecte automatiquement quand vous passez de 1 à 0 (nouveau tour) ou de 0 à 1 (tour arrière).

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

