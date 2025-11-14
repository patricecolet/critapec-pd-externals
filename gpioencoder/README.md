# rpi_encoder_step

External Pure Data pour lire un encodeur rotatif (quadrature) et son commutateur sur les GPIO d’un Raspberry Pi via **libgpiod**.

## Fonctionnalités

- Lecture quadrature A/B avec détection du sens (+/-).
- Pas (`step`), minimum et maximum configurables pour calculer la valeur accumulée.
- Lecture du switch avec rappel périodique de l’état (0 = appuyé si pull-up).
- Thread dédié bloqué sur les interruptions GPIO (libgpiod) et `clock` PD pour l’émission.
- Mode *stub* automatique sur les plateformes sans `libgpiod` (compilation OK mais aucune lecture matérielle).

## Création

```
[rpi_encoder_step chip lineA lineB lineSwitch min max step interval_ms]
```

- `chip` : index du gpiochip (0 par défaut).
- `lineA/B` : numéros BCM des lignes de l’encodeur (17/27 par défaut).
- `lineSwitch` : numéro BCM pour le bouton (22 par défaut).
- `min`, `max` : bornes de la valeur (défaut 0–127).
- `step` : pas appliqué à chaque cran (défaut 1).
- `interval_ms` : période de rafraîchissement PD (défaut 2 ms).

### Messages

- `min N`, `max N`, `step N`, `set N`, `reset`.
- `interval N` pour modifier la période de polling PD.

## Dépendances

- Raspberry Pi OS / Linux avec `libgpiod` 1.x.
- Compilation croisée : ajouter `CFLAGS=-DRPI_ENCODER_FORCE_STUB` pour forcer le mode stub (sans libgpiod).

## Compilation

```
make
```

- Sur Linux, le Makefile ajoute `-lgpiod -lpthread`.
- Sur macOS, la cible se compile en mode stub (sans accès GPIO) pour préparer l’envoi sur le Raspberry Pi.

## Sorties

- **Outlet 0** : valeur accumulée, bornée entre `min` et `max`.
- **Outlet 1** : état du switch (1 = repos avec pull-up, 0 = appuyé).

## Auteurs et licence

GPL-2.0+, Patrice Colet 2025.

