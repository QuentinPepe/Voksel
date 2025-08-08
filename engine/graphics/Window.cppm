module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

export module Graphics.Window;

import Core.Assert;
import Core.Types;

export struct WindowConfig {
    U32 width = 1280;
    U32 height = 720;
    const char *title = "Voksel Engine";
    bool fullscreen = false;
    bool vsync = true;
};

export class Window {
private:
    GLFWwindow *m_Window = nullptr;
    U32 m_Width;
    U32 m_Height;

public:
    explicit Window(const WindowConfig &config) : m_Width{config.width}, m_Height{config.height} {
        assert(glfwInit(), "Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Window = glfwCreateWindow(
            config.width,
            config.height,
            config.title,
            config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
            nullptr
        );

        if (!m_Window) {
            glfwTerminate();
            assert(true, "Failed to create window");
        }
    }

    void RequestClose() const {
        assert(m_Window, "Window must exist");
        glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    ~Window() {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
        }
        glfwTerminate();
    }

    [[nodiscard]] U32 GetWidth() const { return m_Width; }
    [[nodiscard]] U32 GetHeight() const { return m_Height; }

    [[nodiscard]] bool ShouldClose() const {
        return glfwWindowShouldClose(m_Window);
    }

    void PollEvents() {
        glfwPollEvents();
    }

#ifdef _WIN32
    [[nodiscard]] HWND GetWin32Handle() const {
        return glfwGetWin32Window(m_Window);
    }
#endif

    [[nodiscard]] GLFWwindow *GetGLFWHandle() const {
        return m_Window;
    }

    void UpdateDimensions(U32 width, U32 height) {
        m_Width = width;
        m_Height = height;
    }
};
