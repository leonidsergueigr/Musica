#include "I18n.hpp"

#include <cstdlib>
#include <cstring>

namespace {

const char *kEn[] = {
#define MUSICA_STR_EN(name, en, fr) en,
    MUSICA_STRINGS(MUSICA_STR_EN)
#undef MUSICA_STR_EN
};

const char *kFr[] = {
#define MUSICA_STR_FR(name, en, fr) fr,
    MUSICA_STRINGS(MUSICA_STR_FR)
#undef MUSICA_STR_FR
};

static_assert(sizeof(kEn) / sizeof(kEn[0]) ==
                  static_cast<size_t>(Str::Count),
              "English table size mismatch");
static_assert(sizeof(kFr) / sizeof(kFr[0]) ==
                  static_cast<size_t>(Str::Count),
              "French table size mismatch");

Lang g_lang = Lang::En;

} // namespace

void setLang(Lang l) { g_lang = l; }

Lang currentLang() { return g_lang; }

Lang detectLang() {
  for (const char *var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    const char *v = std::getenv(var);
    if (v && *v)
      return std::strncmp(v, "fr", 2) == 0 ? Lang::Fr : Lang::En;
  }
  return Lang::En;
}

const char *tr(Str s) {
  const size_t i = static_cast<size_t>(s);
  if (i >= static_cast<size_t>(Str::Count))
    return "";
  return g_lang == Lang::Fr ? kFr[i] : kEn[i];
}

std::string trf(Str s, std::initializer_list<std::string> args) {
  std::string out = tr(s);
  size_t index = 0;
  for (const std::string &arg : args) {
    const std::string token = "{" + std::to_string(index) + "}";
    size_t pos = 0;
    while ((pos = out.find(token, pos)) != std::string::npos) {
      out.replace(pos, token.size(), arg);
      pos += arg.size();
    }
    ++index;
  }
  return out;
}
