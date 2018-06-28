PKGCONFIG_DEPS = wayland-egl wayland-client

all: shm egl

%: main_%.c bin
	$(CC) $< -o bin/$@ $(shell pkg-config --cflags --libs $(PKGCONFIG_DEPS)) -lEGL -lGLESv2

bin:
	mkdir bin
