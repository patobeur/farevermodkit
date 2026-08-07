#include "ui_modules.h"

namespace fmk {
namespace ui {

void render_bossrun(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered) {
    bool bossrun_has_stats = false;
    std::string detected_kind = "aucun";
    std::string detected_class;
    bool detected_is_boss = false;
    std::string timer_state = "idle";
    std::string timer_value = "00:00:00";
    std::string last_value, average_value, kills_value = "0", wipes_value = "0";
    std::string stats_text, counts_text;
    bool has_stats_text = false, has_counts_text = false;
    
    for (const auto& line : rendered) {
        if (line.rfind("DETECTED|", 0) == 0) {
            const std::size_t sep = line.find('|', 9);
            detected_kind = sep == std::string::npos ? line.substr(9)
                                                     : line.substr(9, sep - 9);
            if (sep != std::string::npos) {
                const std::size_t boss_sep = line.find('|', sep + 1);
                detected_class = boss_sep == std::string::npos
                    ? line.substr(sep + 1)
                    : line.substr(sep + 1, boss_sep - sep - 1);
                detected_is_boss = boss_sep != std::string::npos &&
                                   line.substr(boss_sep + 1) == "boss";
            }
        } else if (line.rfind("TIMER|", 0) == 0) {
            const std::size_t sep = line.find('|', 6);
            timer_state = sep == std::string::npos ? "idle" : line.substr(6, sep - 6);
            if (sep != std::string::npos) timer_value = line.substr(sep + 1);
        } else if (line.rfind("STATS_TEXT|", 0) == 0) {
            stats_text = line.substr(11);
            has_stats_text = true;
            bossrun_has_stats = true;
        } else if (line.rfind("STATS|", 0) == 0) {
            const std::size_t sep = line.find('|', 6);
            if (sep != std::string::npos) {
                last_value = line.substr(6, sep - 6);
                average_value = line.substr(sep + 1);
                bossrun_has_stats = true;
            }
        } else if (line.rfind("COUNTS_TEXT|", 0) == 0) {
            counts_text = line.substr(12);
            has_counts_text = true;
        } else if (line.rfind("COUNTS|", 0) == 0) {
            const std::size_t sep = line.find('|', 7);
            if (sep != std::string::npos) {
                kills_value = line.substr(7, sep - 7);
                wipes_value = line.substr(sep + 1);
            }
        }
    }

    const float default_width = 400.0f;
    const float default_height = bossrun_has_stats ? 190.0f : 166.0f;
    if (ctx.w <= 0.0f) ctx.w = default_width;
    if (ctx.h <= 0.0f) ctx.h = default_height;
    ctx.w = std::clamp(ctx.w, 320.0f, 3840.0f - ctx.x); // Assuming bounds check is done outside
    ctx.h = std::clamp(ctx.h, 166.0f, 2160.0f - ctx.y);

    const bool has_detection = detected_kind != "aucun" && !detected_kind.empty();
    const float name_y = ctx.y + 40.0f;
    fmk::draw_rect(ctx.x + 8, name_y, ctx.w - 16, 32,
                   {0.66f, 0.68f, 0.69f, 0.96f});
    const bool is_boss = detected_is_boss;
    const fmk::Color name_color = !has_detection
        ? fmk::Color{0.28f,0.30f,0.32f,1.0f}
        : (is_boss ? fmk::Color{0.88f,0.16f,0.12f,1.0f}
                   : fmk::Color{0.10f,0.55f,0.22f,1.0f});
    const float name_w = fmk::measure_text(18.0f, detected_kind.c_str());
    const float name_x = ctx.x + (ctx.w - name_w) * 0.5f;
    fmk::draw_text(name_x, name_y + 7, 18.0f, name_color,
                   detected_kind.c_str());

    if (has_detection && !is_boss) {
        const bool tracked = ctx.plugins->memory().boss_tracking_enabled(detected_kind);
        const float toggle_x = ctx.x + ctx.w - 31.0f;
        const float toggle_y = name_y + 4.0f;
        const bool toggle_hot = ctx.in_rect(toggle_x, toggle_y, 24.0f, 24.0f);
        const int texture = tracked ? ctx.bossrun_enabled_tex
                                    : ctx.bossrun_disabled_tex;
        if (texture >= 1)
            fmk::draw_image(texture, toggle_x, toggle_y, 24, 24,
                            0, 0, 1, 1, {1,1,1,1});
        if (ctx.clicked && ctx.dragging.empty() && toggle_hot)
            ctx.plugins->memory().set_boss_tracking_enabled(detected_kind, !tracked);
    }

    const fmk::Color timer_color = timer_state == "running"
        ? fmk::Color{1.0f,0.48f,0.10f,1.0f}
        : (timer_state == "finished"
           ? fmk::Color{1.0f,0.88f,0.20f,1.0f}
           : fmk::Color{1.0f,1.0f,1.0f,1.0f});
    const float timer_w = fmk::measure_text(38.0f, timer_value.c_str());
    fmk::draw_text(ctx.x + (ctx.w - timer_w) * 0.5f, ctx.y + 82,
                   38.0f, timer_color, timer_value.c_str());
    float footer_y = ctx.y + 140.0f;
    if (bossrun_has_stats) {
        const std::string stats = has_stats_text ? stats_text
                : ("Dernier " + last_value + "    Moyen " + average_value);
        const float stats_w = fmk::measure_text(14.0f, stats.c_str());
        fmk::draw_text(ctx.x + (ctx.w - stats_w) * 0.5f, ctx.y + 134,
                       14.0f, {0.72f,0.78f,0.82f,1.0f}, stats.c_str());
        footer_y = ctx.y + 162.0f;
    }
    const std::string counts = has_counts_text ? counts_text
            : ("Victoires " + kills_value + "    Echecs " + wipes_value);
    const float counts_w = fmk::measure_text(13.0f, counts.c_str());
    fmk::draw_text(ctx.x + (ctx.w - counts_w) * 0.5f, footer_y,
                   13.0f, {0.55f,0.65f,0.70f,1.0f}, counts.c_str());
}

} // namespace ui
} // namespace fmk
