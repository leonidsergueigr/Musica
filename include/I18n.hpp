#pragma once

#include <initializer_list>
#include <string>

// UI only: the core does not depend on this module.
enum class Lang { En, Fr };

// String table: (key, English, French). A single list keeps the enum and the
// two language tables in sync.
#define MUSICA_STRINGS(X)                                                     \
  X(AppTitle, "Musica", "Musica")                                             \
  X(QueueTitle, "Queue", "File d'attente")                                    \
  X(CommandsTitle, "Commands", "Commandes")                                   \
  X(PlaylistsTitle, "Playlists", "Playlists")                                 \
  X(AddToPlaylistTitle, "Add to a playlist", "Ajouter à une playlist")        \
  X(HelpTitle, "Help", "Aide")                                                \
  X(InfoTitle, "Track info", "Infos de la piste")                             \
  X(ConfirmTitle, "Confirm", "Confirmation")                                  \
  X(NowPlayingNone, "Nothing playing", "Rien en cours")                       \
  X(UnknownArtist, "Unknown artist", "Artiste inconnu")                       \
  X(UnknownAlbum, "Unknown album", "Album inconnu")                           \
  X(TracksCount, "{0} tracks", "{0} pistes")                                  \
  X(TrackCountOne, "{0} track", "{0} piste")                                  \
  X(FilterIndicator, "filter: {0} ({1} results)",                             \
    "filtre : {0} ({1} résultats)")                                           \
  X(FilterIndicatorOne, "filter: {0} (1 result)",                             \
    "filtre : {0} (1 résultat)")                                           \
  X(NewPlaylistItem, "+ New playlist…", "+ Nouvelle playlist…")               \
  X(TerminalTooSmall, "Terminal too small (min 40x12)",                       \
    "Terminal trop petit (min 40x12)")                                        \
  X(Pause, "Pause", "Pause")                                                  \
  X(Play, "Play", "Lecture")                                                  \
  X(Next, "Next", "Suivant")                                                  \
  X(Previous, "Previous", "Précédent")                                        \
  X(SeekBack, "-5 s", "-5 s")                                                 \
  X(SeekFwd, "+5 s", "+5 s")                                                  \
  X(Navigate, "Navigate", "Naviguer")                                         \
  X(Panel, "Panel", "Panneau")                                                \
  X(AddFile, "+ file", "+ fichier")                                           \
  X(AddFolder, "+ folder", "+ dossier")                                       \
  X(PlaylistsChip, "Playlists", "Playlists")                                  \
  X(HelpChip, "Help", "Aide")                                                 \
  X(Validate, "Confirm", "Valider")                                           \
  X(Cancel, "Cancel", "Annuler")                                              \
  X(LoadChip, "Load", "Charger")                                              \
  X(AddHere, "Add here", "Ajouter ici")                                       \
  X(SaveQueue, "Save queue", "Sauver la file")                                \
  X(Rename, "Rename", "Renommer")                                             \
  X(Delete, "Delete", "Supprimer")                                            \
  X(Close, "Close", "Fermer")                                                 \
  X(CloseHelp, "Close help", "Fermer l'aide")                                 \
  X(SearchChip, "Search", "Chercher")                                         \
  X(InfoChip, "Info", "Infos")                                                \
  X(RemoveChip, "Remove", "Retirer")                                          \
  X(UpChip, "Up", "Monter")                                                   \
  X(DownChip, "Down", "Descendre")                                            \
  X(LanguageChip, "Language", "Langue")                                       \
  X(VolumeLabel, "vol", "vol")                                                \
  X(PromptNewPlaylist, "New playlist", "Nouvelle playlist")                   \
  X(PromptSaveQueue, "Save the queue as", "Sauver la file sous le nom")       \
  X(PromptRename, "Rename", "Renommer")                                       \
  X(PromptSearch, "Search", "Rechercher")                                     \
  X(ErrInvalidName, "Failed: empty, invalid or already used name",            \
    "Échec : nom vide, invalide ou déjà pris")                                \
  X(MsgAddedTo, "Added to “{0}”", "Ajouté à « {0} »")                         \
  X(ErrAddFailed, "Failed to add the track", "Échec de l'ajout")              \
  X(MsgPlaylistQueued, "Playlist added to the queue",                         \
    "Playlist ajoutée à la file")                                             \
  X(MsgQueueSaved, "Queue saved as “{0}”", "File sauvegardée sous « {0} »")   \
  X(MsgRemoved, "Deleted “{0}”", "« {0} » supprimée")                         \
  X(MsgRenamed, "Renamed to “{0}”", "Renommée en « {0} »")                   \
  X(ErrReadFile, "Cannot read: {0}", "Lecture impossible : {0}")              \
  X(ErrNoPlayable, "No playable track", "Aucune piste lisible")               \
  X(MsgSkipped, "Skipped {0} unreadable track(s)",                           \
    "{0} piste(s) illisible(s) ignorée(s)")                                   \
  X(NothingPlayingErr, "Nothing is playing", "Rien n'est en cours de lecture") \
  X(HelpPlayback, "Playback", "Lecture")                                      \
  X(HelpNav, "Navigation", "Navigation")                                      \
  X(HelpMouse, "Mouse", "Souris")                                             \
  X(HkPause, "pause / play", "pause / lecture")                               \
  X(HkNextPrev, "next / previous", "suivant / précédent")                     \
  X(HkSeek, "-5 s / +5 s", "-5 s / +5 s")                                     \
  X(HkVolume, "volume", "volume")                                             \
  X(HkShuffleRepeat, "shuffle / repeat", "shuffle / répétition")              \
  X(HkMove, "down / up", "descendre / monter")                                \
  X(HkPanels, "left / right panel", "panneau gauche / droit")                 \
  X(HkEnter, "play / enter", "jouer / entrer")                                \
  X(HkAdd, "add file / folder", "ajouter fichier / dossier")                  \
  X(HkSearch, "search the active panel", "chercher dans le panneau actif")    \
  X(HkPlManage, "manage playlists", "gérer les playlists")                    \
  X(HkPlAdd, "add the track", "ajouter le morceau")                           \
  X(HkPlPopup, "playlists popup", "popup playlists")                          \
  X(HkPlEdit, "remove / move track", "retirer / déplacer")                    \
  X(HkInfo, "track info", "infos de la piste")                                \
  X(HkClick, "select / button", "sélectionner / bouton")                      \
  X(HkClickLabel, "click", "clic")                                           \
  X(HkDouble, "play / enter", "jouer / entrer")                               \
  X(HkDoubleLabel, "double-click", "double-clic")                            \
  X(HkWheel, "scroll / volume", "défiler / volume")                           \
  X(HkWheelLabel, "wheel", "molette")                                         \
  X(HkDrag, "seek / volume bars", "barres de position")                       \
  X(HkDragLabel, "drag", "glisser")                                           \
  X(HkClose, "? / esc: close   q: quit", "? / esc : fermer   q : quitter")    \
  X(InfoTitleTag, "Title", "Titre")                                           \
  X(InfoArtist, "Artist", "Artiste")                                          \
  X(InfoAlbum, "Album", "Album")                                              \
  X(InfoTrackNo, "Track", "Piste")                                            \
  X(InfoDuration, "Duration", "Durée")                                        \
  X(InfoBitrate, "Bitrate", "Débit")                                          \
  X(InfoPath, "Path", "Chemin")                                               \
  X(ConfirmDelete, "Delete “{0}”?", "Supprimer « {0} » ?")                    \
  X(YesNo, "(y/n)", "(o/n)")                                                  \
  X(MsgLangEn, "Language: English", "Langue : english")                       \
  X(MsgLangFr, "Language: French", "Langue : français")                       \
  X(CliUsage, "Usage: musica [options]", "Utilisation : musica [options]")    \
  X(CliOptions, "Options:", "Options :")                                      \
  X(CliNoMouse, "disable mouse tracking (terminal text selection)",           \
    "désactiver la souris (sélection de texte du terminal)")                  \
  X(CliLang, "interface language: fr or en", "langue de l'interface : fr ou en") \
  X(CliHelp, "show this help", "afficher cette aide")                         \
  X(CliVersion, "show the version", "afficher la version")

enum class Str {
#define MUSICA_STR_ENUM(name, en, fr) name,
  MUSICA_STRINGS(MUSICA_STR_ENUM)
#undef MUSICA_STR_ENUM
      Count
};

void setLang(Lang l);
Lang currentLang();
Lang detectLang(); // LC_ALL, LC_MESSAGES, LANG ("fr" prefix)

const char *tr(Str s);

// Substitutes the {0}, {1}, ... tokens with the given arguments.
std::string trf(Str s, std::initializer_list<std::string> args);
