#include "ui_modules.h"

namespace fmk {
namespace ui {

void render_map(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered) {
    std::string map_status = "En attente des donnees du monde...";
    std::string map_zoom = "1.00";
    for (const auto& line : rendered) {
        if (line.rfind("MAP_STATUS|", 0) == 0) {
            std::string payload = line.substr(11);
            size_t pipe = payload.find('|');
            if (pipe != std::string::npos) {
                map_status = payload.substr(0, pipe);
                map_zoom = payload.substr(pipe + 1);
            } else {
                map_status = payload;
            }
        }
    }
    
    if (ctx.w <= 0.0f) ctx.w = 470.0f;
    if (ctx.h <= 0.0f) ctx.h = 430.0f;
    ctx.w = std::clamp(ctx.w, 360.0f, 3840.0f - ctx.x);
    ctx.h = std::clamp(ctx.h, 320.0f, 2160.0f - ctx.y);

    const float map_button_w = 38.0f;
    const float map_button_h = 38.0f;
    const float map_button_y = ctx.y + ctx.h - map_button_h - 10.0f;
    const float map_gap = 10.0f;
    const float map_buttons_x = ctx.x + 10.0f;
    const bool zoom_out_hot = ctx.in_rect(map_buttons_x, map_button_y, map_button_w, map_button_h);
    const bool zoom_in_hot = ctx.in_rect(map_buttons_x + map_button_w + map_gap,
                                     map_button_y, map_button_w, map_button_h);
    const bool chest_hot = ctx.in_rect(map_buttons_x + 2 * (map_button_w + map_gap),
                                   map_button_y, map_button_w, map_button_h);
    const bool orb_hot = ctx.in_rect(map_buttons_x + 3 * (map_button_w + map_gap),
                                 map_button_y, map_button_w, map_button_h);

    auto map_asset_texture = [&](const char* name) {
        const auto module_assets = ctx.asset_textures.find(status.manifest.id);
        if (module_assets == ctx.asset_textures.end()) return -1;
        const auto it = module_assets->second.find(name);
        return it == module_assets->second.end() ? -1 : it->second;
    };
    const int zoom_out_icon = map_asset_texture("zoom-out.png");
    const int zoom_in_icon = map_asset_texture("zoom-in.png");
    const int chest_icon = map_asset_texture("chests.png");
    const int orb_icon = map_asset_texture("orbs.png");
    
    auto map_button = [&](float x, float w, const char* label, int texture, bool hot) {
        if (texture >= 0)
            fmk::draw_image(texture, x + 2.0f, map_button_y + 2.0f,
                            w - 4.0f, map_button_h - 4.0f, 0, 0, 1, 1,
                            hot ? fmk::Color{1.0f,1.0f,0.78f,1.0f}
                                : fmk::Color{1,1,1,1});
        else
            ctx.button(x, map_button_y, w, label, hot,
                   {0.15f,0.30f,0.46f,1.0f});
    };
    
    map_button(map_buttons_x, map_button_w, "-", zoom_out_icon, zoom_out_hot);
    map_button(map_buttons_x + map_button_w + map_gap, map_button_w,
               "+", zoom_in_icon, zoom_in_hot);
    map_button(map_buttons_x + 2 * (map_button_w + map_gap), map_button_w,
               "Coffres", chest_icon, chest_hot);
    map_button(map_buttons_x + 3 * (map_button_w + map_gap), map_button_w,
               "Orbes", orb_icon, orb_hot);
               
    if (ctx.clicked && ctx.dragging.empty()) {
        if (zoom_out_hot) ctx.plugins->dispatch_event("map_zoom_out");
        else if (zoom_in_hot) ctx.plugins->dispatch_event("map_zoom_in");
        else if (chest_hot) ctx.plugins->dispatch_event("map_chests");
        else if (orb_hot) ctx.plugins->dispatch_event("map_orbs");
    }
    
    // Draw zoom indicator (top right)
    std::string zoom_str = "x" + map_zoom;
    const float zoom_w = fmk::measure_text(16.0f, zoom_str.c_str());
    const float zoom_x = ctx.x + ctx.w - zoom_w - 20.0f;
    const float zoom_y = ctx.y + 44.0f; // Just below title bar
    fmk::draw_rect(zoom_x - 6.0f, zoom_y - 2.0f, zoom_w + 12.0f, 24.0f, {0.0f,0.0f,0.0f,0.8f});
    fmk::draw_text(zoom_x, zoom_y + 2.0f, 16.0f, {1.0f,1.0f,1.0f,1.0f}, zoom_str.c_str());

    // The module image is clipped to content, so redraw the
    // frame edge and resize grip after it to keep the chrome
    // visually above the map.
    fmk::draw_rect_outline(ctx.x, ctx.y, ctx.w, ctx.h,
                           1.5f, {0.30f,0.80f,0.70f,1.0f});
    if (ctx.resize_tex >= 0)
        fmk::draw_image(ctx.resize_tex,
                        ctx.x + ctx.w - 18.0f,
                        ctx.y + ctx.h - 18.0f,
                        16.0f, 16.0f, 0, 0, 1, 1,
                        {1,1,1,1});
}

} // namespace ui
} // namespace fmk
