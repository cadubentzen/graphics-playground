#pragma once

// FIXME: this breaks inheritance. It shouldn't be here.
#define WL_EGL_PLATFORM
#include <EGL/egl.h>
#include <string>

namespace LearningGLES {

class Window {
public:
    Window(const char* title, unsigned width, unsigned height);

    EGLDisplay eglDisplay() { return m_eglDisplay; }
    EGLSurface eglSurface() { return m_eglSurface; }
    EGLContext eglContext() { return m_eglContext; }

protected:
    EGLDisplay m_eglDisplay { nullptr };
    EGLConfig m_eglConfig { nullptr };
    EGLSurface m_eglSurface { nullptr };
    EGLContext m_eglContext { nullptr };

    std::string m_title;
    unsigned m_width { 0 };
    unsigned m_height { 0 };
};

} // namespace LearningGLES
