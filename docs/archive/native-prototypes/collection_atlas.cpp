#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "collection_atlas.h"
#include "overlay.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace fmk {
namespace {

constexpr int kCategories = 13;
const char* kCategoryIds[kCategories] = {
    "appearances", "mounts", "pets", "gliders", "trinkets", "weapons",
    "consumables", "materials", "recipes", "augments", "misc", "creatures", "runes"
};
const char* kCategoryNames[kCategories] = {
    "Appearances", "Mounts", "Pets", "Gliders", "Trinkets", "Weapons",
    "Consumables", "Materials", "Recipes", "Augments", "Misc", "Creatures", "Runes"
};

struct Entry {
    int category = 0;
    std::string id, name, description;
    std::vector<std::string> acquire, tags;
    int rarity = 0;
    int icon = -1;
};

std::vector<Entry> g_entries;
int g_begin[kCategories + 1]{};
int g_texture = -1;
float g_texture_w = 0, g_texture_h = 0;
int g_tab = 0;
std::string g_filter;
float g_scroll[kCategories]{};
bool g_loaded = false;

const Color kRarity[] = {
    {0.80f, 0.82f, 0.86f, 1}, {0.20f, 0.82f, 0.34f, 1},
    {0.24f, 0.58f, 1.00f, 1}, {0.68f, 0.34f, 0.90f, 1},
    {1.00f, 0.67f, 0.10f, 1}
};
const char* kRarityNames[] = {"Common", "Uncommon", "Rare", "Epic", "Legendary"};

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> out;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, delimiter)) out.push_back(part);
    if (!value.empty() && value.back() == delimiter) out.emplace_back();
    return out;
}

bool inside(const AtlasInput& in, float x, float y, float w, float h) {
    return in.cursor_valid && in.mouse_x >= x && in.mouse_x < x + w &&
           in.mouse_y >= y && in.mouse_y < y + h;
}

void icon_uv(int icon, float* u0, float* v0, float* u1, float* v1) {
    const int columns = std::max(1, (int)(g_texture_w / 64.0f));
    const int cx = icon % columns, cy = icon / columns;
    *u0 = (cx * 64 + .5f) / g_texture_w; *v0 = (cy * 64 + .5f) / g_texture_h;
    *u1 = ((cx + 1) * 64 - .5f) / g_texture_w; *v1 = ((cy + 1) * 64 - .5f) / g_texture_h;
}

void wrap(const std::string& text, float size, float width,
          std::vector<std::string>& lines) {
    std::istringstream words(text);
    std::string word, line;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure_text(size, candidate.c_str()) > width) {
            lines.push_back(line); line = word;
        } else line = candidate;
    }
    if (!line.empty()) lines.push_back(line);
}

void tooltip(const Entry& e, float mx, float my, float sw, float sh) {
    std::vector<std::string> lines;
    if (!e.description.empty()) wrap(e.description, 13, 310, lines);
    for (const auto& source : e.acquire) wrap(source, 13, 310, lines);
    const float h = 70.0f + lines.size() * 18.0f;
    float x = std::min(mx + 18.0f, sw - 340.0f);
    float y = std::min(my + 18.0f, sh - h);
    draw_rect(x, y, 340, h, {0.025f, 0.035f, 0.055f, .98f});
    draw_rect_outline(x, y, 340, h, 2, kRarity[e.rarity]);
    draw_text(x + 12, y + 10, 16, kRarity[e.rarity], e.name.c_str());
    draw_text(x + 12, y + 32, 13, {0.70f, 0.82f, 0.92f, 1}, kRarityNames[e.rarity]);
    float ly = y + 54;
    for (const auto& line : lines) {
        draw_text(x + 12, ly, 13, {0.82f, 0.86f, 0.92f, 1}, line.c_str()); ly += 18;
    }
}

} // namespace

bool collection_atlas_init(const std::filesystem::path& dir) {
    std::ifstream input(dir / "farever-atlas.tsv", std::ios::binary);
    if (!input) return false;
    std::vector<Entry> raw;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = split(line, '\t');
        if (fields.size() < 7) continue;
        int category = -1;
        for (int i = 0; i < kCategories; ++i) if (fields[0] == kCategoryIds[i]) category = i;
        if (category < 0) continue;
        Entry e;
        e.category = category; e.id = fields[1]; e.name = fields[2];
        e.rarity = std::clamp(atoi(fields[3].c_str()), 0, 4);
        e.icon = atoi(fields[4].c_str()); e.description = fields[5];
        e.acquire = split(fields[6], '|');
        if (fields.size() >= 9) e.tags = split(fields[8], ',');
        raw.push_back(std::move(e));
    }
    for (int category = 0; category < kCategories; ++category) {
        g_begin[category] = (int)g_entries.size();
        for (auto& e : raw) if (e.category == category) g_entries.push_back(std::move(e));
    }
    g_begin[kCategories] = (int)g_entries.size();

    const auto dds = dir / "farever-atlas-icons.dds";
    std::ifstream header(dds, std::ios::binary);
    unsigned char bytes[20]{};
    header.read((char*)bytes, sizeof(bytes));
    if (header.gcount() == 20 && memcmp(bytes, "DDS ", 4) == 0) {
        uint32_t texture_h = 0, texture_w = 0;
        memcpy(&texture_h, bytes + 12, 4); memcpy(&texture_w, bytes + 16, 4);
        g_texture_h = (float)texture_h; g_texture_w = (float)texture_w;
        g_texture = overlay_load_atlas(dds.string().c_str());
    }
    g_loaded = !g_entries.empty();
    return g_loaded;
}

void collection_atlas_draw(float sw, float sh, float& x, float& y,
                           const AtlasInput& in, bool& begin_drag,
                           std::string& drag_id, float& drag_dx, float& drag_dy) {
    constexpr float w = 820, h = 560, title_h = 32, icon = 48, stride = 54;
    x = std::clamp(x, 0.0f, std::max(0.0f, sw - w));
    y = std::clamp(y, 0.0f, std::max(0.0f, sh - h));
    draw_rect(x, y, w, h, {0.025f, 0.040f, 0.065f, .96f});
    draw_rect(x, y, w, title_h, {0.065f, 0.085f, 0.125f, .99f});
    draw_rect_outline(x, y, w, h, 2, {0.20f, 0.50f, 0.78f, 1});
    draw_text(x + 10, y + 7, 17, {0.92f, 0.94f, 0.98f, 1}, "Collection Atlas");
    draw_text(x + w - 260, y + 8, 12, {0.55f, 0.62f, 0.74f, 1},
              "Reload + bank + offline characters");
    if (in.clicked && inside(in, x, y, w, title_h)) {
        begin_drag = true; drag_id = "blaakan.inventory";
        drag_dx = in.mouse_x - x; drag_dy = in.mouse_y - y;
    }
    if (!g_loaded) {
        draw_text(x + 18, y + 58, 16, {1, .45f, .35f, 1}, "Atlas data unavailable");
        return;
    }

    float tx = x + 8, ty = y + 38;
    for (int category = 0; category < kCategories; ++category) {
        char label[64]; sprintf_s(label, "%s %d", kCategoryNames[category],
                                  g_begin[category + 1] - g_begin[category]);
        const float tw = measure_text(12, label) + 16;
        if (tx + tw > x + w - 8) { tx = x + 8; ty += 25; }
        const bool hot = inside(in, tx, ty, tw, 22);
        draw_rect(tx, ty, tw, 22, category == g_tab ? Color{.14f,.22f,.35f,1}
                                                     : Color{.055f,.07f,.105f,1});
        if (category == g_tab) draw_rect(tx, ty + 20, tw, 2, {.25f,.65f,1,1});
        draw_text(tx + 8, ty + 4, 12, category == g_tab ? Color{.9f,.93f,1,1}
                                                        : Color{.58f,.64f,.75f,1}, label);
        if (in.clicked && hot) { g_tab = category; g_filter.clear(); }
        tx += tw + 3;
    }

    const float search_y = ty + 29;
    draw_rect(x + 10, search_y, 240, 24, {.018f,.025f,.04f,1});
    draw_rect_outline(x + 10, search_y, 240, 24, 1, {.20f,.27f,.38f,1});
    draw_text(x + 18, search_y + 5, 13, {.52f,.58f,.68f,1}, "Search all pages - click here");

    std::vector<std::string> tags;
    for (int i = g_begin[g_tab]; i < g_begin[g_tab + 1]; ++i)
        for (const auto& tag : g_entries[i].tags)
            if (tag.rfind("craft:", 0) && tag.rfind("mastery:", 0) &&
                std::find(tags.begin(), tags.end(), tag) == tags.end()) tags.push_back(tag);
    std::sort(tags.begin(), tags.end());
    float fx = x + 10, fy = search_y + 30;
    for (const auto& tag : tags) {
        const std::string label = tag.substr(tag.find(':') + 1);
        const float fw = measure_text(11, label.c_str()) + 14;
        if (fx + fw > x + w - 10) { fx = x + 10; fy += 22; }
        const bool on = tag == g_filter, hot = inside(in, fx, fy, fw, 19);
        draw_rect(fx, fy, fw, 19, on ? Color{.18f,.32f,.48f,1} : Color{.06f,.075f,.11f,1});
        draw_text(fx + 7, fy + 3, 11, on ? Color{.9f,.94f,1,1} : Color{.58f,.64f,.75f,1}, label.c_str());
        if (in.clicked && hot) g_filter = on ? "" : tag;
        fx += fw + 3;
    }

    const float grid_y = fy + 26, grid_bottom = y + h - 10;
    std::vector<const Entry*> visible;
    for (int i = g_begin[g_tab]; i < g_begin[g_tab + 1]; ++i) {
        if (g_filter.empty() || std::find(g_entries[i].tags.begin(), g_entries[i].tags.end(), g_filter) != g_entries[i].tags.end())
            visible.push_back(&g_entries[i]);
    }
    const int columns = 14;
    const int visible_rows = std::max(1, (int)((grid_bottom - grid_y) / stride));
    const int start_row = (int)g_scroll[g_tab];
    const Entry* hovered = nullptr;
    for (int row = 0; row < visible_rows; ++row) for (int col = 0; col < columns; ++col) {
        const int index = (start_row + row) * columns + col;
        if (index >= (int)visible.size()) break;
        const Entry& e = *visible[index];
        const float ix = x + 10 + col * stride, iy = grid_y + row * stride;
        draw_rect(ix, iy, icon, icon, {.025f,.035f,.055f,1});
        if (g_texture >= 0 && e.icon >= 0) {
            float u0,v0,u1,v1; icon_uv(e.icon,&u0,&v0,&u1,&v1);
            draw_image(g_texture,ix,iy,icon,icon,u0,v0,u1,v1,{.58f,.60f,.64f,1});
        }
        draw_rect_outline(ix, iy, icon, icon, inside(in,ix,iy,icon,icon) ? 2 : 1,
                          inside(in,ix,iy,icon,icon) ? Color{1,1,1,1} : kRarity[e.rarity]);
        if (inside(in,ix,iy,icon,icon)) hovered = &e;
    }
    const int total_rows = ((int)visible.size() + columns - 1) / columns;
    if (total_rows > visible_rows) {
        const float track_x = x + w - 8, track_h = grid_bottom - grid_y;
        draw_rect(track_x, grid_y, 5, track_h, {.08f,.10f,.14f,1});
        draw_rect(track_x, grid_y + track_h * start_row / total_rows, 5,
                  std::max(18.0f, track_h * visible_rows / total_rows), {.28f,.38f,.55f,1});
    }
    if (hovered) tooltip(*hovered, in.mouse_x, in.mouse_y, sw, sh);
}

} // namespace fmk
