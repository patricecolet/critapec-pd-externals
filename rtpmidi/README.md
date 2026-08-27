# rtpmidi

External Pure Data pour auto-connecter une session **RTP-MIDI** (Network MIDI) depuis un patch. Le transport MIDI lui-même reste géré par le système (Core MIDI sur macOS, ALSA sur Linux, WinMM sur Windows) — ce external n'a pour rôle que d'activer la session réseau et de connecter un peer, sans action manuelle de l'utilisateur dans une fenêtre de préférences.

## Messages

- `enable 1` / `enable 0` — active / désactive la session réseau.
- `policy anyone|contacts|none` — politique d'acceptation des connexions entrantes.
- `scan 1` / `scan 0` — démarre / arrête la découverte Bonjour sur `_apple-midi._udp.` dans le domaine `local.`. Chaque peer annoncé produit un message `peer <name> <host> <port>` sur l'outlet ; chaque peer qui disparaît produit `peer-removed <name>`.
- `connect <name-or-host> [port]` — se connecte à un peer. Si `<name>` correspond à un peer actuellement résolu par le scan Bonjour, l'adresse et le port découverts sont utilisés. Sinon `<name>` est traité comme une adresse/hostname directe (port par défaut `5004`). C'est ce qui permet de **se connecter sans connaître l'IP** : après `scan 1`, attends les messages `peer` et utilise le nom directement.
- `disconnect` — retire les peers ajoutés par cet objet (laisse intacts ceux créés ailleurs).
- `bang` — rapporte `enabled <0|1>` puis `connections <n>` sur l'outlet.

**Sortie** (`symbole + liste` sur le seul outlet) : `enabled 0|1`, `policy <name>`, `scanning 0|1`, `peer <name> <host> <port>`, `peer-removed <name>`, `connected <host> <port>`, `disconnected`.

### Exemple : connexion sans IP

```
[scan 1(              -> [rtpmidi] -> [route peer]
                                      |
                              [unpack s s f]
                              (name, host, port du peer découvert)

[connect MonAutreMac(  -> [rtpmidi]
```

Le NSRunLoop Bonjour est pompé par un clock Pd (250 ms), donc les événements `peer` arrivent avec un léger retard mais sans thread supplémentaire.

## Par plateforme

### macOS

Implémenté nativement via `MIDINetworkSession` du framework Core MIDI. Aucun logiciel tiers à installer ; le external active la session et ajoute le peer programmatiquement. L'utilisateur voit ensuite la connexion comme un port MIDI standard dans les préférences MIDI de Pd.

**Déploiement minimum** : macOS 10.15 (API `MIDINetworkSession` annotée 10.15+).

**Attention** : la session réseau Core MIDI est un **singleton par processus**. Plusieurs objets `[rtpmidi]` dans le même patch manipulent tous la même session ; chaque instance ne retire que les hôtes qu'elle a ajoutés (via `disconnect`), mais `enable`/`policy` sont globaux.

### Linux

Installer et lancer [rtpmidid](https://github.com/davidmoreno/rtpmidid) :

```bash
# Debian/Ubuntu
sudo apt install rtpmidid
sudo systemctl enable --now rtpmidid
```

La découverte des peers est faite par Avahi (déjà présent sur la plupart des distributions). Une fois `rtpmidid` actif, les sessions distantes apparaissent comme des ports ALSA que Pd peut ouvrir normalement. `aconnect -l` liste les ports disponibles.

### Windows

Installer le driver [rtpMIDI de Tobias Erichsen](https://www.tobias-erichsen.de/software/rtpmidi.html), créer une session dans son interface, puis connecter le peer distant. Les ports deviennent visibles comme MIDI WinMM standards dans Pd.

## Messages console

À la création, l'objet poste un message indiquant la marche à suivre selon l'OS détecté.

## Licence

GPL-2.0+ — Patrice Colet, 2025.
