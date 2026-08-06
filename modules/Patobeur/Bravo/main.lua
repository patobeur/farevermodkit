-- FareverModKit example plugin.
-- Every callback is optional; the core protects each callback from errors.

function on_init()
    -- Called once when the plugin is loaded.
end

function on_render()
    imgui.text(i18n("message"))
end

function on_event(name, data)
    -- Reserved for future game events.
end

function on_settings()
    -- Optional settings window.
end

function on_shutdown()
    -- Called before the plugin is unloaded.
end