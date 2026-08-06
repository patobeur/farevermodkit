#pragma once

#include <filesystem>
#include <string>

namespace fmk {

struct AtlasInput {
    float mouse_x = 0;
    float mouse_y = 0;
    bool cursor_valid = false;
    bool clicked = false;
};

bool collection_atlas_init(const std::filesystem::path& asset_dir);
void collection_atlas_draw(float screen_w, float screen_h, float& x, float& y,
                           const AtlasInput& input, bool& begin_drag,
                           std::string& drag_id, float& drag_dx, float& drag_dy);

} // namespace fmk
