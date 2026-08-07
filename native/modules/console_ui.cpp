#include "ui_modules.h"
#include <cmath>
#include <algorithm>
#include "input.h" // For input_take_wheel_in

namespace fmk {
namespace ui {

void render_console(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered) {
    if (ctx.w <= 0.0f) ctx.w = 720.0f;
    if (ctx.h <= 0.0f) ctx.h = 430.0f;
    ctx.w = std::clamp(ctx.w, 420.0f, 3840.0f - ctx.x);
    ctx.h = std::clamp(ctx.h, 180.0f, 2160.0f - ctx.y);

    const std::size_t visible_lines = (std::size_t)std::max(
        1.0f, std::floor((ctx.h - 73.0f) / 21.0f));
    static std::size_t console_back = 0;
    static std::size_t console_last_count = 0;
    if (rendered.size() != console_last_count && console_back == 0)
        console_last_count = rendered.size();
        
    const int wheel = fmk::input_take_wheel_in((int)ctx.x, (int)(ctx.y + 34),
                                               (int)ctx.w, (int)(ctx.h - 34));
    fmk::input_set_wheel_rect((int)ctx.x, (int)(ctx.y + 34),
                              (int)ctx.w,
                              (int)(ctx.h - 34));
                              
    const std::size_t max_back = rendered.size() > visible_lines
        ? rendered.size() - visible_lines : 0;
    if (wheel > 0) console_back = std::min(max_back, console_back + (std::size_t)wheel);
    if (wheel < 0) {
        const std::size_t down = (std::size_t)(-wheel);
        console_back = down >= console_back ? 0 : console_back - down;
    }
    console_back = std::min(console_back, max_back);
    
    const std::size_t end_line = rendered.size() - console_back;
    const std::size_t first_line = end_line > visible_lines
        ? end_line - visible_lines : 0;
        
    fmk::draw_rect(ctx.x + 8, ctx.y + 40, ctx.w - 16,
                   ctx.h - 50,
                   {0.015f,0.025f,0.032f,0.98f});
    float line_y = ctx.y + 48.0f;
    for (std::size_t i = first_line; i < end_line; ++i) {
        const std::string line = rendered[i].rfind("LOG|", 0) == 0
            ? rendered[i].substr(4) : rendered[i];
        fmk::draw_text(ctx.x + 16, line_y, 15.0f,
                       {0.62f,0.88f,0.72f,1.0f}, line.c_str());
        line_y += 21.0f;
    }
    const std::string scroll_info = std::to_string(first_line + (rendered.empty()?0:1)) +
        "-" + std::to_string(end_line) + " / " + std::to_string(rendered.size());
    const float scroll_w = fmk::measure_text(13.0f, scroll_info.c_str());
    
    const float plugin_close_x = ctx.x + ctx.w - 30.0f;
    const float scroll_x = plugin_close_x - 10.0f - scroll_w;
    fmk::draw_text(scroll_x, ctx.y + 10, 13.0f,
                   {0.68f,0.76f,0.82f,1.0f}, scroll_info.c_str());
}

} // namespace ui
} // namespace fmk
