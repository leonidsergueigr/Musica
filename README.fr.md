# Musica

Un lecteur de musique en terminal rapide, pilotable au clavier et à la souris, écrit en C++20,
avec pochettes, spectre en temps réel et navigateur de dossiers.

**English: [README.md](README.md)**

## Fonctionnalités

- Navigateur de dossiers et file d'attente côte à côte
- Lecture séquentielle, shuffle et répétition
- Pochettes depuis les tags intégrés (MP3, FLAC, M4A, Ogg) ou `cover.jpg` /
  `folder.png` à côté des fichiers, chargées en arrière-plan
- Spectre multi-lignes en temps réel
- Barres de position et de volume interactives (clic, glisser, molette)
- Recherche `/` en direct dans le panneau actif (insensible à la casse et aux accents)
- Popup d'infos de la piste, édition de la file (retirer, déplacer)
- Playlists M3U : créer, ajouter, charger, renommer, supprimer avec confirmation
- Interface française / anglaise, détectée depuis la locale ou mémorisée
- Les fichiers illisibles sont ignorés avec un message visible
- Souris, et `--no-mouse` pour sélectionner du texte dans le terminal
- Mémorise le dernier dossier, le volume, shuffle, répétition et la langue
- Adapté aux petits terminaux : mise en page compacte sous 80×24, message clair sous 40×12

## Compilation

Nécessite un compilateur C++20, CMake ≥ 3.20, et les dépendances habituelles
de TagLib (zlib, cmake). FTXUI et TagLib sont récupérés automatiquement,
miniaudio est fourni dans le dépôt.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/musica

# optionnel : installer le binaire musica
sudo cmake --install build
```

Une capture d'écran ou un GIF peut être enregistré avec
[vhs](https://github.com/charmbracelet/vhs) puis ajouté au README.

## Configuration et données

- État (dernier dossier, volume, shuffle, répétition, langue) :
  `~/.config/musica/state` (ou `$XDG_CONFIG_HOME/musica/state`)
- Playlists : `~/.config/musica/playlists/` (ou `$XDG_CONFIG_HOME/musica/playlists/`)
- Langue : `--lang fr|en` (prioritaire sur le choix mémorisé), ou touche `L`
  en cours d'exécution
- `--no-mouse` désactive la souris, `--help` et `--version` affichent l'aide

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
| `/` | Chercher dans le panneau actif (`esc` efface) |
| `i` | Infos de la piste |
| `x` | Retirer la piste sélectionnée de la file |
| `J` / `K` | Descendre / monter la piste sélectionnée |
| `p` / `P` | Gérer les playlists / ajouter la piste |
| `?` | Aide |
| `L` | Changer de langue |
| `q` | Quitter |

Dans le popup playlists : `⏎` charger, `a` ajouter la file, `s` sauver la file,
`r` renommer, `d` supprimer (confirmation o/n).

Souris : clic pour sélectionner, double-clic pour jouer, molette pour
défiler, glisser sur les barres de position et de volume, cliquer les puces
de commandes et les boutons.

## Attributions tierces

- [miniaudio](https://github.com/mackron/miniaudio) — domaine public / MIT-0
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) — MIT
- [TagLib](https://github.com/taglib/taglib) — LGPL-2.1 / MPL-1.1
- [stb_image](https://github.com/nothings/stb) — domaine public / MIT

## Licence

La licence de Musica n'est pas encore choisie. MIT et MPL-2.0 sont toutes
deux compatibles avec les dépendances ci-dessus (TagLib est utilisé comme
bibliothèque).
