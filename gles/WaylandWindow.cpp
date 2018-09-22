#include "WaylandWindow.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace LearningGLES {

struct wl_registry_listener WaylandWindow::s_wlRegistryListener = {
    /* global */
    [](void* data, struct wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
        auto& window = *static_cast<WaylandWindow*>(data);
        if (!strcmp(interface, "wl_compositor"))
            window.m_wlCompositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, id, &wl_compositor_interface, 1));
        else if (!strcmp(interface, "wl_shell"))
            window.m_wlShell = static_cast<struct wl_shell*>(wl_registry_bind(registry, id, &wl_shell_interface, 1));
    },
    /* global_remove */
    [](void*, struct wl_registry*, uint32_t) {}
};

struct wl_shell_surface_listener WaylandWindow::s_wlShellSurfaceListener = {
    /* ping */
    [](void* data, struct wl_shell_surface* surface, uint32_t serial) {
        wl_shell_surface_pong(surface, serial);
    },
    /* configure */
    [](void* data, struct wl_shell_surface* surface, uint32_t edges, int32_t width, int32_t height) {
        auto& window = *static_cast<WaylandWindow*>(data);
        wl_egl_window_resize(window.m_wlEGLWindow, width, height, 0, 0);
    },
    /* popup_done */
    [](void* data, struct wl_shell_surface* surface) {}
};

WaylandWindow::WaylandWindow(const char* title, unsigned width, unsigned height)
    : Window(title, width, height)
{
    initWayland();
    wl_shell_surface_set_title(m_wlShellSurface, title);
    initEGL();
}

void WaylandWindow::initWayland()
{
    m_wlDisplay = wl_display_connect(nullptr);

    struct wl_registry* registry = wl_display_get_registry(m_wlDisplay);
    wl_registry_add_listener(registry, &s_wlRegistryListener, this);

    wl_display_dispatch(m_wlDisplay);
    wl_display_roundtrip(m_wlDisplay);

    if (!m_wlCompositor || !m_wlShell) {
        fprintf(stderr, "No compositor or shell.\n");
        wl_display_disconnect(m_wlDisplay);
        exit(1);
    }

    // No error checking for simplicity.
    m_wlSurface = wl_compositor_create_surface(m_wlCompositor);
    m_wlShellSurface = wl_shell_get_shell_surface(m_wlShell, m_wlSurface);
    wl_shell_surface_add_listener(m_wlShellSurface, &s_wlShellSurfaceListener, this);
    wl_shell_surface_set_toplevel(m_wlShellSurface);
}

void WaylandWindow::initEGL()
{
    m_eglDisplay = eglGetDisplay(m_wlDisplay);
    eglInitialize(m_eglDisplay, nullptr, nullptr);

    EGLint attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs;
    eglChooseConfig(m_eglDisplay, attributes, &m_eglConfig, 1, &numConfigs);
    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, nullptr);
    m_wlEGLWindow = wl_egl_window_create(m_wlSurface, m_width, m_height);
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, m_wlEGLWindow, nullptr);
    eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
}

WaylandWindow::~WaylandWindow()
{
    eglDestroySurface(m_eglDisplay, m_eglSurface);
    wl_egl_window_destroy(m_wlEGLWindow);
    wl_shell_surface_destroy(m_wlShellSurface);
    wl_surface_destroy(m_wlSurface);
    eglDestroyContext(m_eglDisplay, m_eglContext);
    eglTerminate(m_eglDisplay);
    wl_display_disconnect(m_wlDisplay);
}

void WaylandWindow::processInputs()
{
    wl_display_dispatch_pending(m_wlDisplay);
}

} // namespace LearningGLES
