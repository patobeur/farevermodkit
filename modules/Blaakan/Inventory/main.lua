-- The native Atlas surface renders this module from the shared item database.
-- Keeping a render line lets older FMK hosts still expose a useful fallback.
function on_init() end
function on_render()
    imgui.text("Collection Atlas")
end
function on_settings() imgui.text(i18n("settings")) end
function on_shutdown() end