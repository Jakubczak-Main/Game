#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

// Basic window settings live in constants so they are easy to change later.
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "RGB Triangle";

// Vertex shader:
// - receives one 2D position and one RGB color per vertex
// - passes the color to the fragment shader
// - writes the final clip-space position to gl_Position
constexpr std::string_view kVertexShader = R"glsl(
#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec3 aColor;

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)glsl";

// Fragment shader:
// - runs for every pixel covered by the triangle
// - receives the interpolated vertex color
// - writes the final pixel color
constexpr std::string_view kFragmentShader = R"glsl(
#version 330 core

in vec3 vColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
)glsl";

struct Vec2 {
    float x;
    float y;
};

struct Color {
    float r;
    float g;
    float b;
};

// This matches the vertex input layout used by glVertexAttribPointer below.
struct Vertex {
    Vec2 position;
    Color color;
};

// Three vertices, each with its own color. OpenGL interpolates these colors
// across the triangle, creating the RGB gradient.
constexpr Vertex kTriangleVertices[] = {
    {{-0.65f, -0.45f}, {1.0f, 0.05f, 0.12f}},
    {{ 0.65f, -0.45f}, {0.0f, 0.85f, 0.30f}},
    {{ 0.00f,  0.65f}, {0.1f, 0.35f, 1.00f}},
};

// In larger OpenGL projects it is very easy to call something in the wrong
// order. This helper reports OpenGL errors close to the call that caused them.
void checkGlError(const char* call, const char* file, int line)
{
    GLenum error = GL_NO_ERROR;
    while ((error = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error 0x" << std::hex << error << std::dec
                  << " after " << call << " at " << file << ':' << line << '\n';
    }
}

// Wrap OpenGL calls in GL_CHECK while learning/debugging. You can remove or
// disable this in a performance-sensitive release build later.
#define GL_CHECK(call)      \
    do {                    \
        call;               \
        checkGlError(#call, __FILE__, __LINE__); \
    } while (false)

void glfwErrorCallback(int code, const char* description)
{
    std::cerr << "GLFW error " << code << ": " << description << '\n';
}

// GLFW calls this whenever the drawable framebuffer changes size, for example
// when resizing the window or moving between monitors with different DPI.
void framebufferSizeCallback(GLFWwindow*, int width, int height)
{
    GL_CHECK(glViewport(0, 0, width, height));
}

// Reads the shader compiler error text so GLSL mistakes are visible in the
// terminal instead of silently producing a black screen.
std::string shaderInfoLog(GLuint shader)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    std::string log(static_cast<std::size_t>(length), '\0');
    if (length > 0) {
        glGetShaderInfoLog(shader, length, nullptr, log.data());
    }

    return log;
}

// Same idea as shaderInfoLog, but for errors while linking shaders together
// into a complete GPU program.
std::string programInfoLog(GLuint program)
{
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    std::string log(static_cast<std::size_t>(length), '\0');
    if (length > 0) {
        glGetProgramInfoLog(program, length, nullptr, log.data());
    }

    return log;
}

// Compiles one GLSL shader object. Shader objects are temporary; after linking
// a program, the individual shader objects can be deleted.
GLuint compileShader(GLenum type, std::string_view source)
{
    const GLuint shader = glCreateShader(type);
    const char* sourceData = source.data();
    const GLint sourceLength = static_cast<GLint>(source.size());

    glShaderSource(shader, 1, &sourceData, &sourceLength);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        const std::string log = shaderInfoLog(shader);
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + log);
    }

    return shader;
}

// Owns GLFW initialization/termination. RAII like this helps bigger projects
// clean up correctly even if something throws an exception during startup.
class GlfwSession {
public:
    GlfwSession()
    {
        glfwSetErrorCallback(glfwErrorCallback);
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("Failed to initialize GLFW.");
        }
    }

    ~GlfwSession()
    {
        glfwTerminate();
    }

    GlfwSession(const GlfwSession&) = delete;
    GlfwSession& operator=(const GlfwSession&) = delete;
};

// Owns a GLFW window and its OpenGL context.
class Window {
public:
    Window(int width, int height, const char* title)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
        // Ask the driver for extra debug information in debug builds.
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (window_ == nullptr) {
            throw std::runtime_error("Failed to create a GLFW window.");
        }

        glfwMakeContextCurrent(window_);
        glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
        // V-sync: swap buffers at monitor refresh rate instead of rendering
        // as fast as possible and wasting CPU/GPU.
        glfwSwapInterval(1);
    }

    ~Window()
    {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
        }
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* handle() const
    {
        return window_;
    }

    bool shouldClose() const
    {
        return glfwWindowShouldClose(window_) == GLFW_TRUE;
    }

    void requestClose() const
    {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

    void swapBuffers() const
    {
        glfwSwapBuffers(window_);
    }

private:
    GLFWwindow* window_ = nullptr;
};

// Owns a linked GLSL program. In a bigger renderer you would usually load the
// shader text from files, but the compile/link/error pattern stays the same.
class ShaderProgram {
public:
    ShaderProgram(std::string_view vertexSource, std::string_view fragmentSource)
    {
        const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        id_ = glCreateProgram();
        glAttachShader(id_, vertexShader);
        glAttachShader(id_, fragmentShader);
        glLinkProgram(id_);

        // The linked program keeps its own compiled code, so these temporary
        // shader handles are no longer needed after linking.
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint success = GL_FALSE;
        glGetProgramiv(id_, GL_LINK_STATUS, &success);
        if (success != GL_TRUE) {
            const std::string log = programInfoLog(id_);
            glDeleteProgram(id_);
            id_ = 0;
            throw std::runtime_error("Shader program linking failed:\n" + log);
        }
    }

    ~ShaderProgram()
    {
        if (id_ != 0) {
            glDeleteProgram(id_);
        }
    }

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& other) noexcept
        : id_(std::exchange(other.id_, 0))
    {
    }

    ShaderProgram& operator=(ShaderProgram&& other) noexcept
    {
        if (this != &other) {
            if (id_ != 0) {
                glDeleteProgram(id_);
            }
            id_ = std::exchange(other.id_, 0);
        }

        return *this;
    }

    void use() const
    {
        GL_CHECK(glUseProgram(id_));
    }

private:
    GLuint id_ = 0;
};

// VAO = Vertex Array Object. It remembers how vertex attributes are wired:
// which buffer is used, where each attribute starts, and how large each one is.
class VertexArray {
public:
    VertexArray()
    {
        glGenVertexArrays(1, &id_);
    }

    ~VertexArray()
    {
        if (id_ != 0) {
            glDeleteVertexArrays(1, &id_);
        }
    }

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    VertexArray(VertexArray&& other) noexcept
        : id_(std::exchange(other.id_, 0))
    {
    }

    VertexArray& operator=(VertexArray&& other) noexcept
    {
        if (this != &other) {
            if (id_ != 0) {
                glDeleteVertexArrays(1, &id_);
            }
            id_ = std::exchange(other.id_, 0);
        }

        return *this;
    }

    void bind() const
    {
        GL_CHECK(glBindVertexArray(id_));
    }

private:
    GLuint id_ = 0;
};

// VBO = Vertex Buffer Object. It stores vertex data on the GPU.
class Buffer {
public:
    Buffer()
    {
        glGenBuffers(1, &id_);
    }

    ~Buffer()
    {
        if (id_ != 0) {
            glDeleteBuffers(1, &id_);
        }
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept
        : id_(std::exchange(other.id_, 0))
    {
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this != &other) {
            if (id_ != 0) {
                glDeleteBuffers(1, &id_);
            }
            id_ = std::exchange(other.id_, 0);
        }

        return *this;
    }

    void bind(GLenum target) const
    {
        GL_CHECK(glBindBuffer(target, id_));
    }

    void upload(GLenum target, const void* data, GLsizeiptr byteSize, GLenum usage) const
    {
        bind(target);
        GL_CHECK(glBufferData(target, byteSize, data, usage));
    }

private:
    GLuint id_ = 0;
};

// Converts OpenGL debug enum values into readable text.
const char* debugSeverityName(GLenum severity)
{
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        return "high";
    case GL_DEBUG_SEVERITY_MEDIUM:
        return "medium";
    case GL_DEBUG_SEVERITY_LOW:
        return "low";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        return "notification";
    default:
        return "unknown";
    }
}

const char* debugTypeName(GLenum type)
{
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        return "error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "deprecated behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "undefined behavior";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "portability";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "performance";
    default:
        return "message";
    }
}

// OpenGL can report driver-side warnings/errors through this callback. It is
// much nicer than hunting for a black screen with no context.
void APIENTRY openGlDebugCallback(
    GLenum,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei,
    const GLchar* message,
    const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    std::cerr << "OpenGL " << debugTypeName(type)
              << " [" << debugSeverityName(severity) << "] "
              << id << ": " << message << '\n';
}

// Enables OpenGL's debug callback when the loaded driver supports OpenGL 4.3.
void enableOpenGlDebugOutput()
{
#ifndef NDEBUG
    if (GLAD_GL_VERSION_4_3 == 0) {
        return;
    }

    GL_CHECK(glEnable(GL_DEBUG_OUTPUT));
    GL_CHECK(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
    GL_CHECK(glDebugMessageCallback(openGlDebugCallback, nullptr));
    GL_CHECK(glDebugMessageControl(
        GL_DONT_CARE,
        GL_DONT_CARE,
        GL_DEBUG_SEVERITY_NOTIFICATION,
        0,
        nullptr,
        GL_FALSE));
#endif
}

// Uploads the triangle vertices and describes the memory layout to OpenGL.
void configureTriangleMesh(const VertexArray& vao, const Buffer& vbo)
{
    vao.bind();
    vbo.upload(
        GL_ARRAY_BUFFER,
        kTriangleVertices,
        static_cast<GLsizeiptr>(sizeof(kTriangleVertices)),
        GL_STATIC_DRAW);

    // Attribute 0 is the vec2 position in the vertex shader.
    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, position))));

    // Attribute 1 is the vec3 color in the vertex shader.
    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, color))));
}

// Keep input handling separate from rendering so the main loop stays readable.
void processInput(const Window& window)
{
    if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        window.requestClose();
    }
}

} // namespace

int main()
{
    try {
        // 1. Initialize the windowing library and create an OpenGL context.
        const GlfwSession glfw;
        const Window window(kWindowWidth, kWindowHeight, kWindowTitle);

        // 2. Load OpenGL function pointers after a context exists.
        const int glVersion = gladLoaderLoadGL();
        if (glVersion == 0) {
            throw std::runtime_error("Failed to load OpenGL with GLAD.");
        }

        std::cout << "OpenGL "
                  << GLAD_VERSION_MAJOR(glVersion) << '.'
                  << GLAD_VERSION_MINOR(glVersion) << '\n';

        enableOpenGlDebugOutput();
        GL_CHECK(glViewport(0, 0, kWindowWidth, kWindowHeight));
        GL_CHECK(glClearColor(0.08f, 0.09f, 0.11f, 1.0f));

        {
            // 3. Create GPU resources: shader program, VAO, and VBO.
            const ShaderProgram shader(kVertexShader, kFragmentShader);
            const VertexArray triangleVao;
            const Buffer triangleVbo;

            configureTriangleMesh(triangleVao, triangleVbo);

            // 4. Main loop: handle input, clear the frame, draw, present.
            while (!window.shouldClose()) {
                processInput(window);

                GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
                shader.use();
                triangleVao.bind();
                GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, 3));

                window.swapBuffers();
                glfwPollEvents();
            }
        }

        gladLoaderUnloadGL();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
