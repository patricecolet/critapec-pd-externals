# rtpmidi

External Pure Data pour auto-connecter une session **RTP-MIDI** (Network MIDI) depuis un patch. Le transport MIDI lui-même reste géré par le système (Core MIDI sur macOS, ALSA sur Linux, WinMM sur Windows) — ce external n'a pour rôle que d'activer la session réseau et de connecter un peer, sans action manuelle de l'utilisateur dans une fenêtre de préférences.

## État

Squelette. La logique Core MIDI (macOS) est en cours. Sur Linux / Windows, le external se charge et affiche un message console pour guider la configuration manuelle.

## Par plateforme

### macOS

Pris en charge nativement (à venir) via `MIDINetworkSession` du framework Core MIDI. Aucun logiciel tiers à installer ; le external active la session et ajoute le peer programmatiquement. L'utilisateur voit ensuite la connexion comme un port MIDI standard dans Pd.

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
