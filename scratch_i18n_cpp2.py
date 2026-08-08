import re

with open(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

new_logic = '''void render_report(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered) {
    if (ctx.w <= 0.0f) ctx.w = 470.0f;
    if (ctx.h <= 0.0f) ctx.h = 168.0f;
    ctx.w = std::clamp(ctx.w, 390.0f, 3840.0f - ctx.x);
    ctx.h = std::clamp(ctx.h, 150.0f, 2160.0f - ctx.y);

    std::string report_status = "En attente d'un personnage...";
    std::string t_save = "Sauvegarder maintenant";
    std::string t_open = "Ouvrir le rapport";
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
                std::size_t s2 = rem.find('|', s1 + 1);
                std::size_t s3 = rem.find('|', s2 + 1);
                std::size_t s4 = rem.find('|', s3 + 1);
                std::size_t s5 = rem.find('|', s4 + 1);
                std::size_t s6 = rem.find('|', s5 + 1);
                std::size_t s7 = rem.find('|', s6 + 1);
                if (s7 != std::string::npos) {
                    t_save = rem.substr(s1 + 1, s2 - s1 - 1);
                    t_open = rem.substr(s2 + 1, s3 - s2 - 1);
                    t_none = rem.substr(s3 + 1, s4 - s3 - 1);
                    t_now = rem.substr(s4 + 1, s5 - s4 - 1);
                    t_ago = rem.substr(s5 + 1, s6 - s5 - 1);
                    t_sec = rem.substr(s6 + 1, s7 - s6 - 1);
                    can_save_flag = rem.substr(s7 + 1) == "1";
                }
            } else {
                report_status = rem;
            }
        }
    }

    fmk::draw_text(ctx.x + 14.0f, ctx.y + 44.0f, 16.0f,
                   {0.68f,0.80f,0.74f,1.0f}, report_status.c_str());
    
    const float save_x = ctx.x + 14.0f;
    const float save_y = ctx.y + 76.0f;
    const float save_w = 200.0f;
    const bool save_hot = can_save_flag && ctx.in_rect(save_x, save_y, save_w, 30.0f);
    ctx.button(save_x, save_y, save_w, t_save.c_str(), save_hot,
           can_save_flag ? fmk::Color{0.16f,0.42f,0.30f,1.0f} : fmk::Color{0.2f,0.2f,0.2f,1.0f});
           
    const float link_x = save_x + save_w + 12.0f;
    const float link_w = ctx.w - (link_x - ctx.x) - 14.0f;
    const bool link_hot = ctx.in_rect(link_x, save_y, link_w, 30.0f);
    ctx.button(link_x, save_y, link_w, t_open.c_str(), link_hot,
           {0.15f,0.30f,0.46f,1.0f});
           
    if (ctx.clicked && ctx.dragging.empty() && save_hot && can_save_flag)
        ctx.plugins->memory().request_report_export();
    if (ctx.clicked && ctx.dragging.empty() && link_hot)
        fmk::report_open();

    const unsigned long saved_tick = fmk::report_last_saved_tick();
    std::string saved = t_none;
    if (saved_tick) {
        const unsigned long seconds = (GetTickCount() - saved_tick) / 1000;
        saved = seconds < 2 ? t_now : t_ago + std::to_string(seconds) + t_sec;
    }
    fmk::draw_text(ctx.x + 14.0f, ctx.y + 118.0f, 14.0f,
                   {0.55f,0.65f,0.70f,1.0f}, saved.c_str());
}'''

text = re.sub(r'void render_report\(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered\) \{[\s\S]*?^\}', new_logic, text, flags=re.MULTILINE)

with open(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
