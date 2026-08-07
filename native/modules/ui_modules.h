#pragma once
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

#include "plugin_host.h"
#include "atlas_ui.h"
#include "input.h"
#include "../../core/src/plugin_manager.h"

namespace fmk {
    namespace ui {
        struct Context {
            float& x;
            float& y;
            float& w;
            float& h;
            bool clicked;
            bool click_down;
            std::string dragging;
            fmk::PluginHost* plugins;
            
            // Mouse properties
            bool cursor_valid;
            int cursor_x;
            int cursor_y;

            // Utilities
            std::function<bool(float, float, float, float)> in_rect;
            std::function<void(float, float, float, const char*, bool, fmk::Color)> button;

            // Textures
            int bossrun_enabled_tex;
            int bossrun_disabled_tex;
            int close_tex;
            int resize_tex;
            int title_icon;
            const std::unordered_map<std::string, std::unordered_map<std::string, int>>& asset_textures;
        };

        void render_bossrun(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered);
        void render_bossrun_history(Context& ctx, const std::vector<std::string>& rendered);
        void render_map(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered);
        void render_report(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered);
        void render_console(Context& ctx, const fmk::PluginStatus& status, const std::vector<std::string>& rendered);
    }
}
