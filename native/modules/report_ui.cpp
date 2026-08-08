#include "ui_modules.h"
#include <windows.h>
#include <array>
#include <string>
#include "report.h"

namespace fmk {
namespace ui {

void render_report(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered) {
    if (ctx.w <= 0.0f) ctx.w = 470.0f;
    if (ctx.h <= 0.0f) ctx.h = 208.0f;
    ctx.w = std::clamp(ctx.w, 390.0f, 3840.0f - ctx.x);
    ctx.h = std::clamp(ctx.h, 150.0f, 2160.0f - ctx.y);

    std::string report_status = "En attente d'un personnage...";
    std::string t_save = "Sauvegarder";
    std::string t_open = "Ouvrir le rapport";
    std::string t_mod = "Dossier Mod";
    std::string t_data = "Dossier Donnees";
    std::string t_none = "Derniere sauvegarde : aucune cette session";
    std::string t_now = "Derniere sauvegarde : a l'instant";
    std::string t_ago = "Derniere sauvegarde : il y a ";
    std::string t_sec = " s";
    bool can_save_flag = false;

    for (const auto& line : rendered) {
        if (line.rfind("REPORT_STATUS|", 0) == 0) {
            std::string rem = line.substr(14);
            std::size_t s1 = rem.find('|');
            if (s1 != std::string::npos) {
                report_status = rem.substr(0, s1);
                can_save_flag = rem.substr(s1 + 1) == "1";
            } else {
                report_status = rem;
            }
        } else if (line.rfind("T_BTNS|", 0) == 0) {
            std::string rem = line.substr(7);
            std::size_t s1 = rem.find('|');
            std::size_t s2 = s1 != std::string::npos ? rem.find('|', s1 + 1) : std::string::npos;
            std::size_t s3 = s2 != std::string::npos ? rem.find('|', s2 + 1) : std::string::npos;
            if (s3 != std::string::npos) {
                t_save = rem.substr(0, s1);
                t_open = rem.substr(s1 + 1, s2 - s1 - 1);
                t_mod = rem.substr(s2 + 1, s3 - s2 - 1);
                t_data = rem.substr(s3 + 1);
            }
        } else if (line.rfind("T_LAST|", 0) == 0) {
            std::string rem = line.substr(7);
            std::size_t s1 = rem.find('|');
            std::size_t s2 = s1 != std::string::npos ? rem.find('|', s1 + 1) : std::string::npos;
            std::size_t s3 = s2 != std::string::npos ? rem.find('|', s2 + 1) : std::string::npos;
            if (s3 != std::string::npos) {
                t_none = rem.substr(0, s1);
                t_now = rem.substr(s1 + 1, s2 - s1 - 1);
                t_ago = rem.substr(s2 + 1, s3 - s2 - 1);
                t_sec = rem.substr(s3 + 1);
            }
        }
    }

    fmk::draw_text(ctx.x + 14.0f, ctx.y + 44.0f, 16.0f, {0.68f,0.80f,0.74f,1.0f}, report_status.c_str());
    
    const float save_x = ctx.x + 14.0f;
    const float save_y = ctx.y + 76.0f;
    const float save_w = 200.0f;
    const bool save_hot = can_save_flag && ctx.in_rect(save_x, save_y, save_w, 30.0f);
    ctx.button(save_x, save_y, save_w, t_save.c_str(), save_hot,
           can_save_flag ? fmk::Color{0.16f,0.42f,0.30f,1.0f} : fmk::Color{0.2f,0.2f,0.2f,1.0f});
           
    const float link_x = save_x + save_w + 12.0f;
    const float link_w = ctx.w - (link_x - ctx.x) - 14.0f;
    const bool link_hot = ctx.in_rect(link_x, save_y, link_w, 30.0f);
    ctx.button(link_x, save_y, link_w, t_open.c_str(), link_hot, {0.15f,0.30f,0.46f,1.0f});
           
    const float btn2_y = save_y + 40.0f;
    const float btn2_w = (ctx.w - 28.0f - 12.0f) / 2.0f;
    const float fmk_x = ctx.x + 14.0f;
    const bool fmk_hot = ctx.in_rect(fmk_x, btn2_y, btn2_w, 30.0f);
    ctx.button(fmk_x, btn2_y, btn2_w, t_mod.c_str(), fmk_hot, {0.2f,0.25f,0.3f,1.0f});

    const float data_x = fmk_x + btn2_w + 12.0f;
    const bool data_hot = ctx.in_rect(data_x, btn2_y, btn2_w, 30.0f);
    ctx.button(data_x, btn2_y, btn2_w, t_data.c_str(), data_hot, {0.2f,0.25f,0.3f,1.0f});

    if (ctx.clicked && ctx.dragging.empty() && save_hot && can_save_flag) ctx.plugins->memory().request_report_export();
    if (ctx.clicked && ctx.dragging.empty() && link_hot) fmk::report_open();
    if (ctx.clicked && ctx.dragging.empty() && fmk_hot) fmk::report_open_mod_folder();
    if (ctx.clicked && ctx.dragging.empty() && data_hot) fmk::report_open_data_folder();

    const unsigned long saved_tick = fmk::report_last_saved_tick();
    std::string saved = t_none;
    if (saved_tick) {
        const unsigned long seconds = (GetTickCount() - saved_tick) / 1000;
        saved = seconds < 2 ? t_now : t_ago + std::to_string(seconds) + t_sec;
    }
    fmk::draw_text(ctx.x + 14.0f, ctx.y + 158.0f, 14.0f, {0.55f,0.65f,0.70f,1.0f}, saved.c_str());
}

} // namespace ui
} // namespace fmk
