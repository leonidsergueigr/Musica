# Musica

Un lecteur de musique en terminal rapide, pilotable au clavier et à la souris, écrit en C++20.

**English: [README.md](README.md)**

## Fonctionnalités

- Navigateur de dossiers et file d'attente côte à côte
- Lecture séquentielle, shuffle et répétition
- Spectre multi-lignes en temps réel
- Barres de position et de volume interactives (clic, glisser, molette)
- Playlists M3U : créer, ajouter, charger, renommer, supprimer
- Interface française / anglaise, détectée depuis la locale
- Souris, et `--no-mouse` pour sélectionner du texte dans le terminal
- Adapté aux petits terminaux : mise en page compacte sous 80×24

## Compilation

Nécessite un compilateur C++20, CMake ≥ 3.20, et les dépendances habituelles
de TagLib (zlib, cmake). FTXUI et TagLib sont récupérés automatiquement,
miniaudio est fourni dans le dépôt.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/musica
```

## Configuration et données

- Playlists : `~/.config/musica/playlists/` (ou `$XDG_CONFIG_HOME/musica/playlists/`)
- Langue : `--lang fr|en`, ou la touche `L` en cours d'exécution

## Touches

| Touche | Action |
| --- | --- |
| `␣` | Lecture / pause |
| `n` / `b` | Piste suivante / précédente |
| `←` / `→` | Reculer / avancer de 5 s |
| `+` / `-` | Volume |
| `s` / `r` | Shuffle / répétition |
| `j` / `k` | Déplacer la sélection |
| `h` / `l` | Panneau navigateur / file |
| `⏎` | Jouer ou entrer |
| `a` / `A` | Ajouter un fichier / dossier |
| `p` / `P` | Gérer les playlists / ajouter la piste |
| `?` | Aide |
| `L` | Changer de langue |
| `q` | Quitter |

Souris : clic pour sélectionner, double-clic pour jouer, molette pour
défiler, glisser sur les barres de position et de volume.

## Licence

Voir `LICENSE` (à choisir) et les attributions tierces dans le README.
