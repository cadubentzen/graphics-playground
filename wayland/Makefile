PKGCONFIG_DEPS = wayland-egl wayland-client
FLAGS =

all: shm egl

%: main_%.c bin/xdg-shell-unstable-v6-protocol.c
	$(CC) $^ -o bin/$@ $(shell pkg-config --cflags --libs $(PKGCONFIG_DEPS)) -lEGL -lGLESv2 -I$(PWD)/bin/ $(FLAGS)

# FIXME: using hard-coded wayland-protocols path
bin/xdg-shell-unstable-v6-protocol.c: bin/xdg-shell-unstable-v6-client-protocol.h
	wayland-scanner code /usr/share/wayland-protocols/unstable/xdg-shell/xdg-shell-unstable-v6.xml bin/xdg-shell-unstable-v6-protocol.c

bin/xdg-shell-unstable-v6-client-protocol.h: bin/
	wayland-scanner client-header /usr/share/wayland-protocols/unstable/xdg-shell/xdg-shell-unstable-v6.xml bin/xdg-shell-unstable-v6-client-protocol.h

bin/:
	mkdir bin
