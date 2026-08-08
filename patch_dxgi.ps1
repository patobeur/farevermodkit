$content = Get-Content native\dxgi_proxy.cpp -Raw

# 1. Add Include
if (-not $content.Contains('#include "modules/ui_modules.h"')) {
    $content = $content -replace '#include "memory/memory_log.h"', "#include `"memory/memory_log.h`"`r`n#include `"modules/ui_modules.h`""
}

# 2. Replace the parsing block
$oldParsing = '                bool bossrun_has_stats = false;
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
                        const std::size_t sep = line.find(''|'', 9);
                        detected_kind = sep == std::string::npos ? line.substr(9)
                                                                 : line.substr(9, sep - 9);
                        if (sep != std::string::npos) {
                            const std::size_t boss_sep = line.find(''|'', sep + 1);
                            detected_class = boss_sep == std::string::npos
                                ? line.substr(sep + 1)
                                : line.substr(sep + 1, boss_sep - sep - 1);
                            detected_is_boss = boss_sep != std::string::npos &&
                                               line.substr(boss_sep + 1) == "boss";
                        }
                    } else if (line.rfind("TIMER|", 0) == 0) {
                        const std::size_t sep = line.find(''|'', 6);
                        timer_state = sep == std::string::npos ? "idle" : line.substr(6, sep - 6);
                        if (sep != std::string::npos) timer_value = line.substr(sep + 1);
                    } else if (line.rfind("STATS_TEXT|", 0) == 0) {
                        stats_text = line.substr(11);
                        has_stats_text = true;
                        bossrun_has_stats = true;
                    } else if (line.rfind("STATS|", 0) == 0) {
                        const std::size_t sep = line.find(''|'', 6);
                        if (sep != std::string::npos) {
                            last_value = line.substr(6, sep - 6);
                            average_value = line.substr(sep + 1);
                            bossrun_has_stats = true;
                        }
                    } else if (line.rfind("COUNTS_TEXT|", 0) == 0) {
                        counts_text = line.substr(12);
                        has_counts_text = true;
                    } else if (line.rfind("COUNTS|", 0) == 0) {
                        const std::size_t sep = line.find(''|'', 7);
                        if (sep != std::string::npos) {
                            kills_value = line.substr(7, sep - 7);
                            wipes_value = line.substr(sep + 1);
                        }
                    }
                }'

$newParsing = '                bool bossrun_has_stats = false;
                for (const auto& line : rendered) {
                    if (line.rfind("STATS_TEXT|", 0) == 0 || line.rfind("STATS|", 0) == 0) {
                        bossrun_has_stats = true; break;
                    }
                }'

if ($content.Contains($oldParsing)) {
    $content = $content.Replace($oldParsing, $newParsing)
}

# 3. Replace the UI drawing block
$startIndex = $content.IndexOf('                if (bossrun_window) {')
$endIndex = $content.IndexOf('                ++window_index;', $startIndex)

if ($startIndex -ge 0 -and $endIndex -gt $startIndex) {
    $oldDrawing = $content.Substring($startIndex, $endIndex - $startIndex)
    
    $newDrawing = '                fmk::ui::Context ctx{
                    pos.x, pos.y, pos.w, pos.h,
                    clicked, click_down, dragging, g_plugins,
                    cursor_valid, cursor.x, cursor.y,
                    in_rect, button,
                    g_bossrun_enabled_texture, g_bossrun_disabled_texture,
                    g_close_icon_texture, g_resize_icon_texture, title_icon,
                    g_module_asset_textures
                };

                if (bossrun_window) {
                    fmk::ui::render_bossrun(ctx, status, rendered);
                } else if (map_window) {
                    fmk::ui::render_map(ctx, status, rendered);
                } else if (report_window) {
                    fmk::ui::render_report(ctx, status, rendered);
                } else if (console_window) {
                    fmk::ui::render_console(ctx, status, rendered);
                } else {
                    float text_y = pos.y + 42.0f;
                    for (const auto& line : rendered) {
                        fmk::draw_text(pos.x + 14.0f, text_y, 18.0f,
                                       {0.45f, 0.90f, 0.65f, 1.0f}, line.c_str());
                        text_y += 24.0f;
                    }
                }
'
    $content = $content.Replace($oldDrawing, $newDrawing)
}

[IO.File]::WriteAllText("D:\farever-mods\farevermodkit\native\dxgi_proxy.cpp", $content)
