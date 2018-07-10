#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "xdg-shell-unstable-v6-client-protocol.h"

static struct {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct zxdg_shell_v6 *xdg_shell;
    struct zxdg_surface_v6 *xdg_surface;
    struct zxdg_toplevel_v6 *xdg_toplevel;
    struct wl_region *region;
    struct wl_egl_window *egl_window;
} wl_data = {0};

static struct {
    EGLDisplay display;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;
} egl_data = {0};

static const int WIDTH = 1280;
static const int HEIGHT = 720;

static void xdg_shell_ping(void *data, struct zxdg_shell_v6* shell, uint32_t serial)
{
    zxdg_shell_v6_pong(shell, serial);
}

static const struct zxdg_shell_v6_listener xdg_shell_listener = {
   xdg_shell_ping
};

static void global_registry_handler(void *d, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    printf("Got a registry event for %s, id = %d\n", interface, id);
    if (!strcmp(interface, "wl_compositor"))
        wl_data.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, version);
    else if (!strcmp(interface, "zxdg_shell_v6")) {
        wl_data.xdg_shell = wl_registry_bind(registry, id, &zxdg_shell_v6_interface, version);
        zxdg_shell_v6_add_listener(wl_data.xdg_shell, &xdg_shell_listener, 0);
    }
}

static void global_registry_remover(void *d, struct wl_registry *registry, uint32_t id)
{
    printf("Got a registry remove event for %d\n", id);
}

static const struct wl_registry_listener listener = {
    global_registry_handler,
    global_registry_remover
};

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
							uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
    printf("Pinged and ponged\n");
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void xdg_surface_configure(void* data, struct zxdg_surface_v6* surface, uint32_t serial)
{
    zxdg_surface_v6_ack_configure(surface, serial);
}

static const struct zxdg_surface_v6_listener xdg_surface_listener = {
    xdg_surface_configure
};

static void xdg_toplevel_configure(void* data, struct zxdg_toplevel_v6* toplevel, int32_t width, int32_t height, struct wl_array* states)
{
    // TODO
}

static void xdg_toplevel_close(void* data, struct zxdg_toplevel_v6* xdg_toplevel)
{

}

static const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
    xdg_toplevel_configure,
    xdg_toplevel_close
};

static void init_wayland()
{
    // 1 - connect to display
    wl_data.display = wl_display_connect(0);
    if (!wl_data.display) {
        fprintf(stderr, "Failed to connect to wayland display :/\n");
        exit(1);
    }
    printf("Connected to display!\n");

    // 2 - add registry listeners
    struct wl_registry *registry = wl_display_get_registry(wl_data.display);
    wl_registry_add_listener(registry, &listener, 0);

    // 3 - we do a roundtrip to wait for a valid compositor
    wl_display_dispatch(wl_data.display);
    wl_display_roundtrip(wl_data.display);

    // 4 - we should have a valid compositor set now
    if (!wl_data.compositor) {
        fprintf(stderr, "No compositor :/\n");
        // FIXME: Doesn't this exit leave the display connected?
        exit(1);
    }
    printf("Compositor bound!\n");

    // 5 - create a surface
    wl_data.surface = wl_compositor_create_surface(wl_data.compositor);
    if (!wl_data.surface) {
        fprintf(stderr, "Can't create surface :/\n");
        exit(1);
    }
    printf("Surface created!\n");

    // 6 - create a xdg shell surface
    wl_data.xdg_surface = zxdg_shell_v6_get_xdg_surface(wl_data.xdg_shell, wl_data.surface);
    if (!wl_data.xdg_surface) {
        fprintf(stderr, "Can't create xdg shell surface :/\n");
        return;
    }
    printf("XDG Shell surface created!\n");

    zxdg_surface_v6_add_listener(wl_data.xdg_surface, &xdg_surface_listener, 0);

    wl_data.xdg_toplevel = zxdg_surface_v6_get_toplevel(wl_data.xdg_surface);

    zxdg_toplevel_v6_add_listener(wl_data.xdg_toplevel, &xdg_toplevel_listener, 0);
    zxdg_toplevel_v6_set_title(wl_data.xdg_toplevel, "WaylandEGL");
    wl_surface_commit(wl_data.surface);
}

static void init_egl()
{
    EGLint major, minor, count, n, size;
    EGLConfig *configs;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    egl_data.display = eglGetDisplay(wl_data.display);
    if (egl_data.display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Can't create EGL display :/\n");
        exit(1);
    }
    printf("Created EGL display!\n");

    if (eglInitialize(egl_data.display, &major, &minor) != EGL_TRUE) {
        fprintf(stderr, "Can't initialize EGL display :/\n");
        exit(1);
    }
    printf("EGL %d.%d!\n", major, minor);

    eglGetConfigs(egl_data.display, 0, 0, &count);
    printf("EGL has %d configs.\n", count);

    configs = calloc(count, sizeof(EGLConfig));

    eglChooseConfig(egl_data.display, config_attribs, configs, count, &n);

    eglGetConfigAttrib(egl_data.display, configs[0], EGL_BUFFER_SIZE, &size);
    printf("Buffer size for config is %d.\n", size);

    eglGetConfigAttrib(egl_data.display, configs[0], EGL_RED_SIZE, &size);
    printf("Red size for config is %d.\n", size);

    eglGetConfigAttrib(egl_data.display, configs[0], EGL_ALPHA_SIZE, &size);
    printf("Alpha size for config is %d.\n", size);

    // Go home with the first config
    egl_data.config = configs[0];

    egl_data.context = eglCreateContext(egl_data.display, egl_data.config, EGL_NO_CONTEXT, context_attribs);
}

static void create_window()
{
    wl_data.egl_window = wl_egl_window_create(wl_data.surface, WIDTH, HEIGHT);
    if (wl_data.egl_window == EGL_NO_SURFACE) {
        fprintf(stderr, "Can't create EGL window :/\n");
        exit(1);
    }
    printf("Created EGL window!\n");

    egl_data.surface = eglCreateWindowSurface(egl_data.display, egl_data.config, wl_data.egl_window, 0);

    if (eglMakeCurrent(egl_data.display, egl_data.surface, egl_data.surface, egl_data.context))
        printf("Made context current!\n");
    else {
        fprintf(stderr, "Failed to make context current :/\n");
        exit(1);
    }

    glClearColor(0, 0, 0.1, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (eglSwapBuffers(egl_data.display, egl_data.surface))
        printf("Swapped buffers!\n");
    else {
        fprintf(stderr, "Failed to swap buffers :/\n");
        exit(1);
    }
}

static void clear_wayland()
{
    wl_display_disconnect(wl_data.display);
}

int main(int argc, char *argv[])
{
    init_wayland();
    init_egl();
    create_window();

    while (wl_display_dispatch(wl_data.display) != -1)
    {
    }

    clear_wayland();
    return 0;
}