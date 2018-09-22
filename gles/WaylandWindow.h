#pragma once

#include "Window.h"
#include <wayland-client.h>
#include <wayland-egl.h>

namespace LearningGLES {

class WaylandWindow : public Window {
public:
    WaylandWindow(const char* title, unsigned width, unsigned height);
    ~WaylandWindow();

    void processInputs();

private:
    void initWayland();
    void initEGL();

    static struct wl_registry_listener s_wlRegistryListener;
    static struct wl_shell_surface_listener s_wlShellSurfaceListener;

    struct wl_display* m_wlDisplay { nullptr };
    struct wl_compositor* m_wlCompositor { nullptr };
    struct wl_surface* m_wlSurface { nullptr };
    struct wl_shell* m_wlShell { nullptr };
    struct wl_shell_surface* m_wlShellSurface { nullptr };
    struct wl_region* m_wlRegion { nullptr };
    struct wl_egl_window* m_wlEGLWindow { nullptr };
};

} // namespace LearningGLES
