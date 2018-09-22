#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <wayland-egl.h>
#include <wayland-client.h>

static struct {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct wl_shell *shell;
    struct wl_shell_surface *shell_surface;
    void *shm_data;
    struct wl_buffer *buffer;
    struct wl_shm *shm;
    uint32_t format;
    struct wl_callback *frame_callback;
} data = {0};

static const int WIDTH = 1280;
static const int HEIGHT = 720;

static void shm_format(void *d, struct wl_shm *shm, uint32_t format)
{
    static uint8_t ran = 0;

    printf("Received shm format: %d\n", format);
    // Choose the first valid format it receives.
    if (!ran) {
        printf("Chosen format is %d\n", format);
        data.format = format;
        ran = 1;
    }
}

struct wl_shm_listener shm_listener = { shm_format };

static void global_registry_handler(void *d, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    printf("Got a registry event for %s, id = %d\n", interface, id);
    if (!strcmp(interface, "wl_compositor"))
        data.compositor = wl_registry_bind(registry, id, &wl_compositor_interface, version);
    else if (!strcmp(interface, "wl_shell"))
        data.shell = wl_registry_bind(registry, id, &wl_shell_interface, version);
    else if (!strcmp(interface, "wl_shm")) {
        data.shm = wl_registry_bind(registry, id, &wl_shm_interface, version);
        wl_shm_add_listener(data.shm, &shm_listener, 0);
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

static void handle_ping(void *d, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
    printf("Pinged and ponged\n");
}

static void handle_configure(void *d, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
    printf("Received configure event on shell surface: edges = %d, width = %d, height = %d\n", edges, width, height);
}

static void handle_popup_done(void *d, struct wl_shell_surface *shell_surface)
{
    printf("Received popup done event on shell surface.\n");
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};

// Yeah, try to delete the declaration here here and you'll see why you have to put it back.
static const struct wl_callback_listener frame_listener;

static void init_wayland()
{
    // 1 - connect to display
    data.display = wl_display_connect(0);
    if (!data.display) {
        fprintf(stderr, "Failed to connect to wayland display :/\n");
        exit(1);
    }
    printf("Connected to display!\n");

    // 2 - add registry listeners
    struct wl_registry *registry = wl_display_get_registry(data.display);
    wl_registry_add_listener(registry, &listener, 0);

    // 3 - we do a roundtrip to wait for a valid compositor
    wl_display_dispatch(data.display);
    wl_display_roundtrip(data.display);

    // 4 - we should have a valid compositor set now
    if (!data.compositor) {
        fprintf(stderr, "No compositor :/\n");
        // FIXME: Doesn't this exit leave the display connected?
        exit(1);
    }
    printf("Compositor bound!\n");

    // 5 - create a surface
    data.surface = wl_compositor_create_surface(data.compositor);
    if (!data.surface) {
        fprintf(stderr, "Can't create surface :/\n");
        exit(1);
    }
    printf("Surface created!\n");

    // 6 - create a shell surface
    data.shell_surface = wl_shell_get_shell_surface(data.shell, data.surface);
    if (!data.shell_surface) {
        fprintf(stderr, "Can't create shell surface :/\n");
        exit(1);
    }
    printf("Shell surface created!\n");

    // 7 - set shell surface to top level
    wl_shell_surface_set_toplevel(data.shell_surface);

    // 8 - register listene for shell surface
    wl_shell_surface_add_listener(data.shell_surface, &shell_surface_listener, 0);

    // 9 - register frame callback listener
    data.frame_callback = wl_surface_frame(data.surface);
    wl_callback_add_listener(data.frame_callback, &frame_listener, 0);
}

static int set_cloexec_or_close(int fd)
{
    long flags;

    if (fd == -1)
        return -1;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1)
        goto err;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
        goto err;

    return fd;

err:
    close(fd);
    return -1;
}

static int create_tmpfile_cloexec(char *tmpname)
{
    int fd;

#ifdef HAVE_MKOSTEMP
    fd = mkostemp(tmpname, O_CLOEXEC);
    if (fd >= 0)
        unlink(tmpname);
#else
    fd = mkstemp(tmpname);
    if (fd >= 0) {
        fd = set_cloexec_or_close(fd);
        unlink(tmpname);
    }
#endif

    return fd;
}

int os_create_anonymous_file(off_t size)
{
    static const char template[] = "/shared-XXXXXX";
    const char *path;
    char *name;
    int fd;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path) {
        errno = ENOENT;
        return -1;
    }

    name = malloc(strlen(path) + sizeof(template));
    if (!name) {
        errno = ENOMEM;
        return -1;
    }

    strcpy(name, path);
    strcat(name, template);

    fd = create_tmpfile_cloexec(name);

    free(name);

    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static struct wl_buffer *create_buffer()
{
    struct wl_shm_pool *pool;
    int stride = WIDTH * 4;
    int size = stride * HEIGHT;
    int fd;
    struct wl_buffer *buffer;

    fd = os_create_anonymous_file(size);
    if (fd < 0) {
        fprintf(stderr, "Failed to create file with %d bytes: %m\n", size);
        exit(1);
    }

    data.shm_data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data.shm_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        exit(1);
    }

    pool = wl_shm_create_pool(data.shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT, stride, data.format);
    wl_shm_pool_destroy(pool);

    return buffer;
}

static void create_window()
{
    data.buffer = create_buffer();

    wl_surface_attach(data.surface, data.buffer, 0, 0);
    wl_surface_commit(data.surface);
}

static void paint_pixels(int ht)
{
    static uint32_t pixel_value = 0x00;

    int n;
    uint32_t *pixel = data.shm_data;
    uint32_t draw_color = 0;

    switch (data.format) {
    case WL_SHM_FORMAT_ARGB8888:
        draw_color = pixel_value;
        break;
    case WL_SHM_FORMAT_XRGB8888:
        draw_color = 0xFFFFFF - pixel_value;
        break;
    // else, leave it black.
    default:
        break;
    }

    for (n = 0; n < WIDTH * ht; ++n)
        *pixel++ = pixel_value;

    if (pixel_value < 0x100)
        pixel_value += 0x01; // varying blue
    else if (pixel_value < 0x10000)
        pixel_value += 0x100; // varying green
    else if (pixel_value < 0x1000000)
        pixel_value += 0x10000;
    else
        pixel_value = 0x00;
}

static void redraw(void *d, struct wl_callback *callback, uint32_t time)
{
    static int ht = 0;
    wl_callback_destroy(data.frame_callback);
    if (ht == 0)
        ht = HEIGHT;
    wl_surface_damage(data.surface, 0, 0, WIDTH, ht);
    paint_pixels(ht--);
    data.frame_callback = wl_surface_frame(data.surface);
    wl_surface_attach(data.surface, data.buffer, 0, 0);
    wl_callback_add_listener(data.frame_callback, &frame_listener, 0);
    wl_surface_commit(data.surface);
}

static const struct wl_callback_listener frame_listener = { redraw };

static void clear_wayland()
{
    wl_display_disconnect(data.display);
}

int main(int argc, char *argv[])
{
    init_wayland();
    create_window();
    redraw(0, 0, 0);

    while (wl_display_dispatch(data.display) != -1)
    {
    }

    clear_wayland();
    return 0;
}
