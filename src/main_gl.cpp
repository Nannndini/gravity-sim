#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Simulation.h"
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <map>

// ── Shaders ──────────────────────────────────────────────────────────────────
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 projection;
uniform vec2 offset;
uniform float scale;
void main() {
    vec2 pos = aPos * scale + offset;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 color;
void main() {
    FragColor = color;
}
)";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error:\n" << infoLog << "\n";
    }
    return shader;
}

GLuint createProgram() {
    GLuint vShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vShader);
    glAttachShader(prog, fShader);
    glLinkProgram(prog);
    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return prog;
}

// ── Helpers ──────────────────────────────────────────────────────────────────
struct RenderData { GLuint VAO, VBO; };

RenderData createCircle(float radius, int segments) {
    std::vector<float> vertices;
    vertices.push_back(0.0f); // Center
    vertices.push_back(0.0f);
    for (int i = 0; i <= segments; ++i) {
        float angle = i * 2.0f * 3.14159265f / segments;
        vertices.push_back(cos(angle) * radius);
        vertices.push_back(sin(angle) * radius);
    }
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    return {VAO, VBO};
}

void getOrthoMatrix(float left, float right, float bottom, float top, float zNear, float zFar, float* mat) {
    for (int i = 0; i < 16; ++i) mat[i] = 0.0f;
    mat[0]  = 2.0f / (right - left);
    mat[5]  = 2.0f / (top - bottom);
    mat[10] = -2.0f / (zFar - zNear);
    mat[12] = -(right + left) / (right - left);
    mat[13] = -(top + bottom) / (top - bottom);
    mat[14] = -(zFar + zNear) / (zFar - zNear);
    mat[15] = 1.0f;
}

struct Color { float r, g, b; };

std::map<std::string, Color> bodyColors = {
    {"Sun",     {1.0f, 0.9f, 0.2f}},
    {"Earth",   {0.2f, 0.5f, 1.0f}},
    {"Mars",    {0.9f, 0.3f, 0.1f}},
    {"Moon",    {0.8f, 0.8f, 0.8f}},
    {"Jupiter", {0.8f, 0.6f, 0.3f}}
};

// ── Main ─────────────────────────────────────────────────────────────────────
int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

    int W = 1000, H = 800;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* win = glfwCreateWindow(W, H, "Gravity Sim v0.2", nullptr, nullptr);
    if (!win) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n"; return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    
    GLuint shader = createProgram();
    glUseProgram(shader);
    
    GLint projLoc   = glGetUniformLocation(shader, "projection");
    GLint colorLoc  = glGetUniformLocation(shader, "color");
    GLint offsetLoc = glGetUniformLocation(shader, "offset");
    GLint scaleLoc  = glGetUniformLocation(shader, "scale");

    RenderData circle1 = createCircle(1.0f, 40);
    
    GLuint dynVAO, dynVBO;
    glGenVertexArrays(1, &dynVAO);
    glGenBuffers(1, &dynVBO);
    glBindVertexArray(dynVAO);
    glBindBuffer(GL_ARRAY_BUFFER, dynVBO);
    // Allocate 40000 floats (sufficient for lines)
    glBufferData(GL_ARRAY_BUFFER, 40000 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Setup Physics
    Simulation sim(0.01);
    sim.addBody({"Sun",    {0,   0, 0}, {0,    0,   0}, 1000.0, 18.0});
    sim.addBody({"Earth",  {200, 0, 0}, {0,    3.16,0},    1.0,  6.0});
    sim.addBody({"Mars",   {300, 0, 0}, {0,    2.58,0},    0.5,  5.0});
    sim.addBody({"Moon",   {206, 0, 0}, {0,    3.90,0},   0.01,  3.0});
    sim.addBody({"Jupiter",{420, 0, 0}, {0,    2.18,0},    5.0, 10.0});

    std::map<std::string, std::vector<Vec3>> trails;
    const size_t MAX_TRAIL = 300;

    while (!glfwWindowShouldClose(win)) {
        for (int i = 0; i < 5; ++i) {
            sim.stepVerlet();
            for (const auto& b : sim.bodies()) {
                trails[b.name].push_back(b.pos);
                if (trails[b.name].size() > MAX_TRAIL) {
                    trails[b.name].erase(trails[b.name].begin());
                }
            }
        }

        glfwGetFramebufferSize(win, &W, &H);
        glViewport(0, 0, W, H);
        glClearColor(0.03f, 0.02f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float aspect = (float)W / H;
        float viewH = 550.0f;
        float viewW = viewH * aspect;
        
        float ortho[16];
        getOrthoMatrix(-viewW, viewW, -viewH, viewH, -1.0f, 1.0f, ortho);
        
        glUseProgram(shader);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, ortho);
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform1f(scaleLoc, 1.0f);

        glBindVertexArray(dynVAO);
        glBindBuffer(GL_ARRAY_BUFFER, dynVBO);

        // 1. Draw Spacetime Grid
        std::vector<float> gridLines;
        int cols = 30, rows = 30;
        float dx = (viewW * 2.0f) / cols;
        float dy = (viewH * 2.0f) / rows;
        
        for (int row = 0; row <= rows; ++row) {
            for (int col = 0; col <= cols; ++col) {
                float wx = col * dx - viewW;
                float wy = row * dy - viewH;
                
                auto applyWarp = [&](float px, float py, float& outX, float& outY) {
                    float warpX = 0, warpY = 0;
                    for (const auto& b : sim.bodies()) {
                        float ddx = (float)b.pos.x - px;
                        float ddy = (float)b.pos.y - py;
                        float dist2 = ddx*ddx + ddy*ddy + 1.0f;
                        float strength = fmin(b.mass * 800.0 / dist2, 60.0);
                        warpX += strength * ddx / sqrt(dist2);
                        warpY += strength * ddy / sqrt(dist2);
                    }
                    outX = px + warpX;
                    outY = py + warpY;
                };

                float p1x, p1y;
                applyWarp(wx, wy, p1x, p1y);

                if (col < cols) {
                    float p2x, p2y; applyWarp((col+1)*dx - viewW, wy, p2x, p2y);
                    gridLines.push_back(p1x); gridLines.push_back(p1y);
                    gridLines.push_back(p2x); gridLines.push_back(p2y);
                }
                if (row < rows) {
                    float p2x, p2y; applyWarp(wx, (row+1)*dy - viewH, p2x, p2y);
                    gridLines.push_back(p1x); gridLines.push_back(p1y);
                    gridLines.push_back(p2x); gridLines.push_back(p2y);
                }
            }
        }
        
        if (gridLines.size() * sizeof(float) > 40000 * sizeof(float)) {
            glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, gridLines.size() * sizeof(float), gridLines.data());
        glUniform4f(colorLoc, 0.3f, 0.2f, 0.6f, 0.4f);
        glDrawArrays(GL_LINES, 0, gridLines.size() / 2);

        // 2. Draw Trails
        for (const auto& b : sim.bodies()) {
            const auto& t = trails[b.name];
            if (t.size() < 2) continue;
            
            std::vector<float> tPoints;
            for(size_t i = 0; i < t.size() - 1; ++i) {
                tPoints.push_back(t[i].x);   tPoints.push_back(t[i].y);
                tPoints.push_back(t[i+1].x); tPoints.push_back(t[i+1].y);
            }
            glBufferSubData(GL_ARRAY_BUFFER, 0, tPoints.size() * sizeof(float), tPoints.data());
            
            Color c = bodyColors.count(b.name) ? bodyColors[b.name] : Color{1,1,1};
            glUniform4f(colorLoc, c.r, c.g, c.b, 0.6f);
            glDrawArrays(GL_LINES, 0, tPoints.size() / 2);
        }

        // 3. Draw Bodies
        glBindVertexArray(circle1.VAO);
        for (const auto& b : sim.bodies()) {
            Color c = bodyColors.count(b.name) ? bodyColors[b.name] : Color{1,1,1};
            
            // Outer glow
            glUniform2f(offsetLoc, b.pos.x, b.pos.y);
            glUniform1f(scaleLoc, b.radius * 2.2f);
            glUniform4f(colorLoc, c.r, c.g, c.b, 0.15f);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 42);

            // Solid inner part
            glUniform1f(scaleLoc, b.radius);
            glUniform4f(colorLoc, c.r, c.g, c.b, 1.0f);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 42);
        }

        glfwSwapBuffers(win);
        glfwPollEvents();

        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(win, true);
    }

    glfwTerminate();
    return 0;
}
