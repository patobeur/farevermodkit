import os

with open(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

header = '''#include "ui_modules.h"
#include <windows.h>
#include <array>
#include <string>
#include "report.h"

std::string detect_game_locale();

static const char* t(const char* fr, const char* en, const char* es) {
    static std::string loc = detect_game_locale();
    if (loc == "fr-FR") return fr;
    if (loc == "es-ES") return es;
    return en;
}

'''

text = text.replace('#include "ui_modules.h"\n#include <windows.h>\n#include <array>\n#include <string>\n#include "report.h" // For fmk::report_open() and fmk::report_last_saved_tick()\n', header)

# replace strings
text = text.replace('"Sauvegarder maintenant"', 't("Sauvegarder maintenant", "Save now", "Guardar ahora")')
text = text.replace('"Ouvrir le rapport"', 't("Ouvrir le rapport", "Open report", "Abrir reporte")')
text = text.replace('"Dossier Mod (FMK)"', 't("Dossier Mod (FMK)", "Mod Folder (FMK)", "Carpeta Mod (FMK)")')
text = text.replace('"Dossier Donnees"', 't("Dossier Donnees", "Data Folder", "Carpeta Datos")')
text = text.replace('"Derniere sauvegarde : aucune cette session"', 't("Derniere sauvegarde : aucune cette session", "Last save: none this session", "Ultima guardia: ninguna esta sesion")')
text = text.replace('"Derniere sauvegarde : a l\'instant"', 't("Derniere sauvegarde : a l\'instant", "Last save: just now", "Ultima guardia: hace un instante")')
text = text.replace('"Derniere sauvegarde : il y a " + std::to_string(seconds) + " s"', 'std::string(t("Derniere sauvegarde : il y a ", "Last save: ", "Ultima guardia: hace ")) + std::to_string(seconds) + " s"')

with open(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

