#include "ui_modules.h"
#include <windows.h>
#include "report.h" // For fmk::report_open() and fmk::report_last_saved_tick()

namespace fmk {
namespace ui {

void render_report(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered) {
    if (ctx.w <= 0.0f) ctx.w = 470.0f;
    if (ctx.h <= 0.0f) ctx.h = 168.0f;
    ctx.w = std::clamp(ctx.w, 390.0f, 3840.0f - ctx.x);
    ctx.h = std::clamp(ctx.h, 150.0f, 2160.0f - ctx.y);

    std::string report_status = "En attente d'un personnage...";
    for (const auto& line : rendered) {
        if (line.rfind("REPORT_STATUS|", 0) == 0)
            report_status = line.substr(14);
    }
    fmk::draw_text(ctx.x + 14.0f, ctx.y + 44.0f, 16.0f,
                   {0.68f,0.80f,0.74f,1.0f}, report_status.c_str());
    
    const float save_x = ctx.x + 14.0f;
    const float save_y = ctx.y + 76.0f;
    const float save_w = 200.0f;
    const bool save_hot = ctx.in_rect(save_x, save_y, save_w, 30.0f);
    ctx.button(save_x, save_y, save_w, "Sauvegarder maintenant", save_hot,
           {0.16f,0.42f,0.30f,1.0f});
           
    const float link_x = save_x + save_w + 12.0f;
    const float link_w = ctx.w - (link_x - ctx.x) - 14.0f;
    const bool link_hot = ctx.in_rect(link_x, save_y, link_w, 30.0f);
    ctx.button(link_x, save_y, link_w, "Ouvrir le rapport", link_hot,
           {0.15f,0.30f,0.46f,1.0f});
           
    if (ctx.clicked && ctx.dragging.empty() && save_hot)
        ctx.plugins->memory().request_report_export();
    if (ctx.clicked && ctx.dragging.empty() && link_hot)
        fmk::report_open();

    const unsigned long saved_tick = fmk::report_last_saved_tick();
    std::string saved = "Derniere sauvegarde : aucune cette session";
    if (saved_tick) {
        const unsigned long seconds = (GetTickCount() - saved_tick) / 1000;
        saved = seconds < 2 ? "Derniere sauvegarde : a l'instant"
            : "Derniere sauvegarde : il y a " + std::to_string(seconds) + " s";
    }
    fmk::draw_text(ctx.x + 14.0f, ctx.y + 118.0f, 14.0f,
                   {0.55f,0.65f,0.70f,1.0f}, saved.c_str());
}

} // namespace ui
} // namespace fmk
