#pragma once

#include "overlay.h"

namespace fmk {

struct WindowFrameInput {
    bool cursor_valid = false;
    float mouse_x = 0;
    float mouse_y = 0;
    bool clicked = false;
};

struct WindowFrameSpec {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    const char* title = "";
    int title_icon = -1;
    int close_icon = -1;
    float title_height = 34.0f;
    Color background{0.04f, 0.07f, 0.09f, 0.94f};
    Color title_background{0.10f, 0.18f, 0.23f, 0.98f};
    Color border{0.30f, 0.80f, 0.70f, 1.0f};
    bool resizable = false;
    int resize_icon = -1;
};

struct WindowFrameResult {
    bool close_hot = false;
    bool close_clicked = false;
    bool title_pressed = false;
    bool resize_hot = false;
    bool resize_pressed = false;
    float content_x = 0;
    float content_y = 0;
    float content_width = 0;
    float content_height = 0;
};

inline bool window_point_inside(const WindowFrameInput& input,
                                float x, float y, float w, float h) {
    return input.cursor_valid && input.mouse_x >= x && input.mouse_x < x + w &&
           input.mouse_y >= y && input.mouse_y < y + h;
}

inline WindowFrameResult draw_window_frame(const WindowFrameSpec& spec,
                                            const WindowFrameInput& input) {
    WindowFrameResult result;
    const float close_x = spec.x + spec.width - 30.0f;
    const float close_y = spec.y + 6.0f;
    result.close_hot = window_point_inside(input, close_x, close_y, 22.0f, 22.0f);
    result.close_clicked = input.clicked && result.close_hot;
    result.title_pressed = input.clicked && !result.close_hot &&
        window_point_inside(input, spec.x, spec.y, spec.width, spec.title_height);
    result.resize_hot = spec.resizable && window_point_inside(
        input, spec.x + spec.width - 20.0f, spec.y + spec.height - 20.0f,
        20.0f, 20.0f);
    result.resize_pressed = input.clicked && result.resize_hot;
    result.content_x = spec.x;
    result.content_y = spec.y + spec.title_height;
    result.content_width = spec.width;
    result.content_height = spec.height - spec.title_height;

    draw_rect(spec.x, spec.y, spec.width, spec.height, spec.background);
    draw_rect(spec.x, spec.y, spec.width, spec.title_height, spec.title_background);
    draw_rect_outline(spec.x, spec.y, spec.width, spec.height, 2.0f, spec.border);
    if (spec.title_icon >= 1)
        draw_image(spec.title_icon, spec.x + 6.0f, spec.y + 4.0f, 26.0f, 26.0f,
                   0, 0, 1, 1, {1,1,1,1});
    draw_text(spec.x + (spec.title_icon >= 1 ? 40.0f : 14.0f), spec.y + 8.0f,
              18.0f, {1,1,1,1}, spec.title);
    if (spec.close_icon >= 1)
        draw_image(spec.close_icon, close_x, close_y, 22.0f, 22.0f,
                   0, 0, 1, 1,
                   result.close_hot ? Color{1.0f,0.72f,0.72f,1.0f}
                                    : Color{1,1,1,1});
    else
        draw_text(close_x + 5.0f, close_y + 1.0f, 16.0f, {1,1,1,1}, "x");

    if (spec.resizable) {
        const float grip_x = spec.x + spec.width - 18.0f;
        const float grip_y = spec.y + spec.height - 18.0f;
        if (spec.resize_icon >= 1)
            draw_image(spec.resize_icon, grip_x, grip_y, 16, 16, 0, 0, 1, 1,
                       result.resize_hot ? Color{1,1,0.75f,1} : Color{1,1,1,0.8f});
        else {
            const Color grip = result.resize_hot ? Color{0.8f,0.95f,1,1}
                                                 : Color{0.45f,0.60f,0.66f,0.9f};
            draw_triangle(spec.x + spec.width - 3.0f, spec.y + spec.height - 3.0f,
                          grip_x, spec.y + spec.height - 3.0f,
                          spec.x + spec.width - 3.0f, grip_y, grip);
        }
    }
    return result;
}

} // namespace fmk