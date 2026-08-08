import os

def update_ui(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    text = text.replace('if (ctx.h <= 0.0f) ctx.h = 168.0f;', 'if (ctx.h <= 0.0f) ctx.h = 208.0f;')

    target = '''    if (ctx.clicked && ctx.dragging.empty() && link_hot)
        fmk::report_open();'''

    new_buttons = target + '''

    const float btn2_y = save_y + 40.0f;
    const float btn2_w = (ctx.w - 28.0f - 12.0f) / 2.0f;
    const float fmk_x = ctx.x + 14.0f;
    const bool fmk_hot = ctx.in_rect(fmk_x, btn2_y, btn2_w, 30.0f);
    ctx.button(fmk_x, btn2_y, btn2_w, "Dossier Mod (FMK)", fmk_hot, {0.2f,0.25f,0.3f,1.0f});

    const float data_x = fmk_x + btn2_w + 12.0f;
    const bool data_hot = ctx.in_rect(data_x, btn2_y, btn2_w, 30.0f);
    ctx.button(data_x, btn2_y, btn2_w, "Dossier Donnees", data_hot, {0.2f,0.25f,0.3f,1.0f});

    if (ctx.clicked && ctx.dragging.empty() && fmk_hot) fmk::report_open_mod_folder();
    if (ctx.clicked && ctx.dragging.empty() && data_hot) fmk::report_open_data_folder();'''

    text = text.replace(target, new_buttons)
    text = text.replace('fmk::draw_text(ctx.x + 14.0f, ctx.y + 118.0f, 14.0f,', 'fmk::draw_text(ctx.x + 14.0f, ctx.y + 158.0f, 14.0f,')

    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    print("Updated report_ui.cpp")

update_ui(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp')
