#include "WaylandWindow.h"
#include <GLES2/gl2.h>

using namespace LearningGLES;

static void draw(WaylandWindow& window)
{
    glClearColor(1.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(window.eglDisplay(), window.eglSurface());
}

int main(int argc, char* argv[])
{
    WaylandWindow window("green", 1280, 720);

    while (true) {
        window.processInputs();
        draw(window);
    }

    return 0;
}
