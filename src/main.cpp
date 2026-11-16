#include "AudioEngine.hpp"
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  // Vérifier qu'un fichier a bien été passé en argument
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <chemin_vers_fichier_audio>"
              << std::endl;
    return -1;
  }

  std::string filename = argv[1];

  AudioEngine engine;
  if (!engine.isInitialized()) {
    std::cerr << "Error to initialise engine." << std::endl;
    return -1;
  }

  if (!engine.load(filename)) {
    std::cerr << "Impossible to charge file: " << filename << std::endl;
    return -1;
  }

  // Récupération et affichage des métadonnées
  std::cout << "Durée   : " << static_cast<int>(engine.getDuration()) << "s"
            << std::endl;
  std::cout << "===========================" << std::endl;

  engine.play();

  float volume = 1.0f;
  char cmd;

  while (std::cin >> cmd && cmd != 'q') {
    if (cmd == 'p') {
      engine.pause();
      std::cout << "Musique en pause" << std::endl;

    } else if (cmd == 'r') {
      engine.play();
      std::cout << "Reprise de la lecture." << std::endl;

    } else if (cmd == 's') {
      engine.pause();
      engine.gotoSecond(0.0f);
      std::cout << "Musique stoppee et remise au debut." << std::endl;

    } else if (cmd == '+') {
      volume += 0.1f;
      engine.setVolume(volume);
      std::cout << "Volume : " << volume << std::endl;

    } else if (cmd == '-') {
      volume = (volume > 0.1f) ? volume - 0.1f : 0.0f;
      engine.setVolume(volume);
      std::cout << "Volume : " << volume << std::endl;
    }
  }

  return 0;
}
