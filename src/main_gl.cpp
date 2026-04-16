#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h>
#include "Simulation.h"
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <map>

// ── 3D Math Engine ───────────────────────────────────────────────────────────
struct Mat4 {
    float m[16];
    Mat4() { for(int i=0;i<16;++i) m[i] = (i%5==0)?1.0f:0.0f; }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for(int c=0; c<4; ++c)
            for(int ro=0; ro<4; ++ro)
                r.m[c*4+ro] = m[0*4+ro]*o.m[c*4+0] + m[1*4+ro]*o.m[c*4+1] + 
                              m[2*4+ro]*o.m[c*4+2] + m[3*4+ro]*o.m[c*4+3];
        return r;
    }
};

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

Mat4 perspective(float fovRad, float aspect, float nearZ, float farZ) {
    Mat4 r; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 0.0f;
    float tanHalfFovy = tan(fovRad / 2.0f);
    r.m[0] = 1.0f / (aspect * tanHalfFovy);
    r.m[5] = 1.0f / (tanHalfFovy);
    r.m[10] = -(farZ + nearZ) / (farZ - nearZ);
    r.m[11] = -1.0f;
    r.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    return r;
}

Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = (center - eye).normalized();
    Vec3 u = up.normalized();
    Vec3 s = cross(f, u).normalized();
    u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] =-f.x; r.m[6] =-f.y; r.m[10] =-f.z;
    r.m[12] = -s.dot(eye);
    r.m[13] = -u.dot(eye);
    r.m[14] = f.dot(eye);
    return r;
}

Mat4 translate(const Vec3& v) {
    Mat4 r;
    r.m[12] = v.x; r.m[13] = v.y; r.m[14] = v.z;
    return r;
}

Mat4 scale(float s) {
    Mat4 r;
    r.m[0] = s; r.m[5] = s; r.m[10] = s;
    return r;
}

Mat4 inverseTransposeScaleTranslate(const Vec3& pos, float s) {
    // Specifically computed inverse-transpose of our basic model matrix. 
    // Since rotation is identity and uniform scale is applied, it's just scale^-1
    Mat4 r;
    r.m[0] = 1.0f/s; r.m[5] = 1.0f/s; r.m[10] = 1.0f/s; // Transpose of diagonal is itself
    // translation parts drop because transpose moves them, and inverse of translation has them in right column
    // Actually, for pure translation + uniform scale, (M^-1)^T just scales normals by 1/s.
    return r;
}

// ── Camera Globals ───────────────────────────────────────────────────────────
Vec3 cameraPos = {0.0, -1100.0, 300.0};
Vec3 cameraFront = {0.0, 1.0, -0.27};
Vec3 cameraUp = {0.0, 0.0, 1.0};
float yaw = 90.0f;
float pitch = -15.0f;
float lastX = 500, lastY = 400;
bool firstMouse = true;
double deltaTime = 0.0;
double lastFrameTime = 0.0;
bool isMouseLocked = true;
bool wasTabPressed = false;

// Sandbox Mode Globals
bool isPaused = false;
bool wasKPressed = false;
bool isSpawning = false;
Simulation* globalSim = nullptr; // For callbacks

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (!globalSim) return;
    
#ifdef __EMSCRIPTEN__
    if (action == GLFW_PRESS) {
        EM_ASM(
            var canvas = document.getElementById('canvas');
            if (canvas && document.pointerLockElement !== canvas) {
                canvas.requestPointerLock();
            }
        );
    }
#endif

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            Vec3 pos = cameraPos + cameraFront * 150.0f; // Shoot from camera
            Vec3 vel = {cameraFront.x * 6.0, cameraFront.y * 6.0, cameraFront.z * 6.0}; // Projectile speed
            globalSim->addBody({"Spawned", pos, vel, 2.0, 5.0});
            isSpawning = true;
        } else if (action == GLFW_RELEASE) {
            isSpawning = false;
        }
    }
    if (isSpawning && button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            auto& bodies = globalSim->getMutableBodies();
            if (!bodies.empty()) {
                bodies.back().mass *= 1.2;
                bodies.back().radius = std::cbrt(bodies.back().mass) * 8.0f;
            }
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if(!isMouseLocked) {
        firstMouse = true;
        return;
    }
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos; lastY = ypos;

    float sensitivity = 0.1f;
    yaw += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    Vec3 front;
    front.x = cos(yaw * 3.14159f / 180.0) * cos(pitch * 3.14159f / 180.0);
    front.y = sin(yaw * 3.14159f / 180.0) * cos(pitch * 3.14159f / 180.0);
    front.z = sin(pitch * 3.14159f / 180.0);
    cameraFront = front.normalized();
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
        glfwSetWindowShouldClose(window, true);
        
    // Toggle mouse lock using TAB
    bool isTabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
    if (isTabPressed && !wasTabPressed) {
        isMouseLocked = !isMouseLocked;
        if (isMouseLocked) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    wasTabPressed = isTabPressed;

    if (!isMouseLocked) return;

    float cameraSpeed = 500.0f * deltaTime;
    bool shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    if (shiftPressed) cameraSpeed *= 4.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += cameraFront * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos = cameraPos - (cameraFront * cameraSpeed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos = cameraPos - (cross(cameraFront, cameraUp).normalized() * cameraSpeed);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += cross(cameraFront, cameraUp).normalized() * cameraSpeed;

    // Pause toggle
    bool kPressed = glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS;
    if (kPressed && !wasKPressed) isPaused = !isPaused;
    wasKPressed = kPressed;

    // Keyboard-only Spawning (Hold SPACE to spawn, M to increase mass)
    static bool wasSpace = false;
    bool spaceDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spaceDown && !wasSpace && globalSim) {
        Vec3 pos = cameraPos + cameraFront * 1200.0f;
        globalSim->addBody({"Spawned", pos, {0,0,0}, 15.0, 20.0});
        isSpawning = true;
    }
    if (!spaceDown && wasSpace) {
        // If space is released and mouse isn't holding, let go
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
            isSpawning = false;
        }
    }
    wasSpace = spaceDown;

    static double lastM = 0;
    if (isSpawning && glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && globalSim) {
        if (glfwGetTime() - lastM > 0.1) { // Throttle growth rate to match repeated clicking
            auto& bodies = globalSim->getMutableBodies();
            if(!bodies.empty()){
                bodies.back().mass *= 1.2;
                bodies.back().radius = std::cbrt(bodies.back().mass) * 8.0f;
            }
            lastM = glfwGetTime();
        }
    }

    // Sandbox positioning
    if (isSpawning && globalSim) {
        auto& bodies = globalSim->getMutableBodies();
        if (!bodies.empty()) {
            float moveSpeed = bodies.back().radius * 0.15f * (shiftPressed ? 4.0f : 1.0f);
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) bodies.back().pos.y += moveSpeed;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) bodies.back().pos.y -= moveSpeed;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) bodies.back().pos.x += moveSpeed;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) bodies.back().pos.x -= moveSpeed;
        }
    }
}

// ── Shaders ──────────────────────────────────────────────────────────────────
// Grid Shader applies dynamic warping via uniform arrays (gravity fields)
const char* gridVertexShaderSource = 
#ifdef __EMSCRIPTEN__
"#version 300 es\n"
"precision highp float;\n"
#else
"#version 330 core\n"
#endif
R"(layout (location = 0) in vec2 aPos;
uniform mat4 projection;
uniform mat4 view;
uniform int numBodies;
uniform vec3 bodyPos[50];
uniform float bodyMass[50];

out float gridDepth;

void main() {
    float z = 0.0;
    float warpX = 0.0;
    float warpY = 0.0;
    
    for(int i = 0; i < numBodies; i++) {
        vec2 diff = bodyPos[i].xy - aPos;
        float dist2 = dot(diff, diff) + 1.0;
        float dist = sqrt(dist2);
        
        // pull grid points horizontally
        float pull = min(bodyMass[i] * 800.0 / dist2, 60.0);
        warpX += pull * diff.x / dist;
        warpY += pull * diff.y / dist;
        
        // Dynamically compute massive overlapping black-hole spacetime funnels without singularities
        // By using an inverse-distance falloff, the ripples smoothly add together when bodies merge
        float warpZ = (bodyMass[i] * 6.0) / (dist * 0.05 + 1.0);
        z -= warpZ;
    }
    
    vec2 pos = aPos + vec2(warpX, warpY);
    gl_Position = projection * view * vec4(pos.x, pos.y, z, 1.0);
    gridDepth = z;
}
)";

const char* gridFragmentShaderSource = 
#ifdef __EMSCRIPTEN__
"#version 300 es\n"
"precision highp float;\n"
#else
"#version 330 core\n"
#endif
R"(in float gridDepth;
out vec4 FragColor;
void main() {
    // Pure white grid lines with beautiful fading opacity based on depth
    float opacity = 0.65 + (gridDepth / 400.0);
    opacity = clamp(opacity, 0.15, 0.85); // Boosted opacity for WebGL thin lines
    FragColor = vec4(1.0, 1.0, 1.0, opacity);
}
)";

const char* trailVertexShaderSource = 
#ifdef __EMSCRIPTEN__
"#version 300 es\n"
"precision highp float;\n"
#else
"#version 330 core\n"
#endif
R"(
layout (location = 0) in vec3 aPos;
uniform mat4 view;
uniform mat4 proj;
void main() {
    gl_Position = proj * view * vec4(aPos, 1.0);
}
)";

const char* trailFragmentShaderSource = 
#ifdef __EMSCRIPTEN__
"#version 300 es\n"
"precision highp float;\n"
#else
"#version 330 core\n"
#endif
R"(
out vec4 FragColor;
uniform vec4 trailColor;
void main() {
    FragColor = trailColor;
}
)";

// Sphere Shader mimics video lighting
const char* sphereVertexShaderSource = 
#ifdef __EMSCRIPTEN__
"#version 300 es\n"
"precision highp float;\n"
#else
"#version 330 core\n"
#endif
R"(layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 normalMatrix;
out vec3 FragPos;
out vec3 Normal;
void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(normalMatrix) * aNormal;  
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* sphereFragmentShaderSource = 
#ifdef __EMSCRIPTEN__
"#version 300 es\n"
"precision highp float;\n"
#else
"#version 330 core\n"
#endif
R"(in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform vec3 lightPos; 
uniform vec3 viewPos;
uniform vec4 objectColor;
uniform bool isSun;

void main() {
    if (isSun) {
        // Massive over-exposure for bloom effect
        FragColor = vec4(objectColor.rgb * 100.0, objectColor.a);
        return;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    // Diffuse shading
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * objectColor.rgb * 1.2;
    
    // Minimal Ambient for the dark side
    vec3 ambient = 0.05 * objectColor.rgb;
    
    // Specular highlight for nice 3D pop
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * vec3(1.0);  
    
    FragColor = vec4(ambient + diffuse + specular, objectColor.a);
}
)";

#include <stdio.h> // Add for printf
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024]; glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        printf("Shader compile error: %s\n", infoLog);
#ifdef __EMSCRIPTEN__
        EM_ASM({ alert('Shader compile error: ' + UTF8ToString($0)); }, infoLog);
#endif
    }
    return shader;
}

GLuint createProgram(const char* vs, const char* fs) {
    GLuint vShader = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fShader = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vShader); glAttachShader(prog, fShader);
    glLinkProgram(prog);
    int success; glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024]; glGetProgramInfoLog(prog, 1024, nullptr, infoLog);
        printf("Program link error: %s\n", infoLog);
#ifdef __EMSCRIPTEN__
        EM_ASM({ alert('Program link error: ' + UTF8ToString($0)); }, infoLog);
#endif
    }
    glDeleteShader(vShader); glDeleteShader(fShader);
    return prog;
}

// ── Global State & Uniforms ───────────────────────────────────────────────────
GLuint trailShader;
GLuint trailVAO, trailVBO;
GLuint objShader;
GLuint gridShader;
GLuint gridVAO = 0, gridVBO = 0;

// ── Sphere Mesh Generation ───────────────────────────────────────────────────
struct RenderData { GLuint VAO, VBO, count; };
struct Vertex { float x, y, z, nx, ny, nz; };

RenderData createSphere(int sectorCount, int stackCount) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float radius = 1.0f;
    float sectorStep = 2 * 3.14159265f / sectorCount;
    float stackStep = 3.14159265f / stackCount;

    for(int i = 0; i <= stackCount; ++i) {
        float stackAngle = 3.14159265f / 2 - i * stackStep;        
        float xy = radius * cosf(stackAngle);             
        float z = radius * sinf(stackAngle);              
        for(int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * sectorStep;           
            float x = xy * cosf(sectorAngle);             
            float y = xy * sinf(sectorAngle);             
            vertices.push_back({x, y, z, x/radius, y/radius, z/radius});
        }
    }

    for(int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1), k2 = k1 + sectorCount + 1;
        for(int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if(i != 0) {
                indices.push_back(k1); indices.push_back(k2); indices.push_back(k1 + 1);
            }
            if(i != (stackCount-1)) {
                indices.push_back(k1 + 1); indices.push_back(k2); indices.push_back(k2 + 1);
            }
        }
    }

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    return {VAO, VBO, (GLuint)indices.size()};
}

// ── Setup ────────────────────────────────────────────────────────────────────
struct Color { float r, g, b; };
std::map<std::string, Color> bodyColors = {
    {"Sun",     {1.0f, 0.9f, 0.2f}},
    {"Earth",   {0.2f, 0.5f, 1.0f}},
    {"Mars",    {0.9f, 0.3f, 0.1f}},
    {"Moon",    {0.8f, 0.8f, 0.8f}},
    {"Jupiter", {0.8f, 0.6f, 0.3f}}
};

int W = 1000, H = 800;
GLFWwindow* win = nullptr;
RenderData sphereMesh;
std::vector<float> gridLines;
Simulation* emscriptenSim = nullptr; // Global container for the Simulation instance

void emulateMainLoop() {
    double currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrameTime;
    lastFrameTime = currentFrame;

    processInput(win);

    if (!isPaused && emscriptenSim) {
        Vec3 lockedPos = {0,0,0};
        if (isSpawning && !emscriptenSim->bodies().empty()) {
            emscriptenSim->getMutableBodies().back().vel = {0,0,0};
            lockedPos = emscriptenSim->bodies().back().pos;
        }
        
        for (int i = 0; i < 50; ++i) {
            emscriptenSim->stepVerlet();
        }

        if (isSpawning && !emscriptenSim->bodies().empty()) {
            emscriptenSim->getMutableBodies().back().pos = lockedPos;
            emscriptenSim->getMutableBodies().back().vel = {0,0,0};
        }
    }

    glfwGetFramebufferSize(win, &W, &H);
    
#ifdef __EMSCRIPTEN__
    // Fix aspect ratio egg-stretching permanently!
    EM_ASM({
        var canvas = document.getElementById('canvas');
        if (canvas.width != window.innerWidth || canvas.height != window.innerHeight) {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
        }
    });
    W = EM_ASM_INT({ return canvas.width; });
    H = EM_ASM_INT({ return canvas.height; });
#endif
    
    glViewport(0, 0, W, H);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f); // Dark space
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Mat4 proj = perspective(45.0f * 3.14159f / 180.0f, (float)W / H, 0.1f, 10000.0f);
    Mat4 view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    if (emscriptenSim) {
        // Upload bodies list to grid shader for warping
        glUseProgram(gridShader);
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "projection"), 1, GL_FALSE, proj.m);
        glUniformMatrix4fv(glGetUniformLocation(gridShader, "view"), 1, GL_FALSE, view.m);
        glUniform1i(glGetUniformLocation(gridShader, "numBodies"), emscriptenSim->bodies().size());
        
        for(size_t i = 0; i < emscriptenSim->bodies().size(); ++i) {
            std::string nStr = "bodyPos[" + std::to_string(i) + "]";
            std::string mStr = "bodyMass[" + std::to_string(i) + "]";
            glUniform3f(glGetUniformLocation(gridShader, nStr.c_str()), 
                        (float)emscriptenSim->bodies()[i].pos.x, (float)emscriptenSim->bodies()[i].pos.y, (float)emscriptenSim->bodies()[i].pos.z);
            glUniform1f(glGetUniformLocation(gridShader, mStr.c_str()), (float)emscriptenSim->bodies()[i].mass);
        }

        // Draw Spacetime Grid
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, gridLines.size() / 2);

        // Draw Planets
        glUseProgram(objShader);
        glUniformMatrix4fv(glGetUniformLocation(objShader, "projection"), 1, GL_FALSE, proj.m);
        glUniformMatrix4fv(glGetUniformLocation(objShader, "view"), 1, GL_FALSE, view.m);
        glUniform3f(glGetUniformLocation(objShader, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        
        // Find Sun for Light position
        Vec3 sunPos = {0,0,0};
        for (const auto& b : emscriptenSim->bodies()) {
            if (b.name == "Sun" || b.name == "Sun-Earth" || b.mass >= 500) { sunPos = b.pos; break; }
        }
        glUniform3f(glGetUniformLocation(objShader, "lightPos"), sunPos.x, sunPos.y, sunPos.z);

        glBindVertexArray(sphereMesh.VAO);
        for (const auto& b : emscriptenSim->bodies()) {
            Color c = bodyColors.count(b.name) ? bodyColors[b.name] : Color{0.8f,0.8f,0.8f};
            
            Mat4 S = scale(b.radius);
            Mat4 T = translate(b.pos);
            Mat4 model = T * S;
            Mat4 normalMat = inverseTransposeScaleTranslate(b.pos, b.radius);

            glUniformMatrix4fv(glGetUniformLocation(objShader, "model"), 1, GL_FALSE, model.m);
            glUniformMatrix4fv(glGetUniformLocation(objShader, "normalMatrix"), 1, GL_FALSE, normalMat.m);
            glUniform4f(glGetUniformLocation(objShader, "objectColor"), c.r, c.g, c.b, 1.0f);
            
            bool isSun = (b.name == "Sun" || b.mass >= 500);
            glUniform1i(glGetUniformLocation(objShader, "isSun"), isSun);

            glDrawElements(GL_TRIANGLES, sphereMesh.count, GL_UNSIGNED_INT, 0);
        }
        
        // 3. Draw Trails behind planets!
        glUseProgram(trailShader);
        glUniformMatrix4fv(glGetUniformLocation(trailShader, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(trailShader, "proj"), 1, GL_FALSE, proj.m);
        
        glBindVertexArray(trailVAO);
        for (const auto& b : emscriptenSim->bodies()) {
            if (b.trail.size() < 2) continue;
            
            std::vector<float> lineData;
            lineData.reserve(b.trail.size() * 3);
            for (const auto& p : b.trail) {
                lineData.push_back(p.x); lineData.push_back(p.y); lineData.push_back(p.z);
            }
            glBindBuffer(GL_ARRAY_BUFFER, trailVBO);
            glBufferData(GL_ARRAY_BUFFER, lineData.size() * sizeof(float), lineData.data(), GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            
            Color c = bodyColors.count(b.name) ? bodyColors[b.name] : Color{0.8f,0.8f,0.8f};
            glUniform4f(glGetUniformLocation(trailShader, "trailColor"), c.r, c.g, c.b, 0.5f);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDrawArrays(GL_LINE_STRIP, 0, lineData.size() / 3);
            glDisable(GL_BLEND);
        }
    }
    
    glfwSwapBuffers(win);
    glfwPollEvents();
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    
    win = glfwCreateWindow(W, H, "Gravity Sim 3D v0.4", nullptr, nullptr);
    if (!win) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetMouseButtonCallback(win, mouseButtonCallback);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Lock mouse inside

#ifndef __EMSCRIPTEN__
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n"; return -1;
    }
#endif

    glEnable(GL_DEPTH_TEST); // Enable 3D depth buffer
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Compile Shaders
    gridShader = createProgram(gridVertexShaderSource, gridFragmentShaderSource);
    objShader = createProgram(sphereVertexShaderSource, sphereFragmentShaderSource);
    trailShader = createProgram(trailVertexShaderSource, trailFragmentShaderSource);

    // Setup Trail geometry
    glGenVertexArrays(1, &trailVAO);
    glGenBuffers(1, &trailVBO);
    
    sphereMesh = createSphere(36, 18);
    
    // Create high-resolution dynamic grid mesh
    int cols = 120; int rows = 120; // Massive high resolution for beautiful curved funnels
    float gridSize = 3000.0f;
    float dx = gridSize * 2.0f / cols;
    float dy = gridSize * 2.0f / rows;
    for (int row = 0; row <= rows; ++row) {
        for (int col = 0; col <= cols; ++col) {
            float wx = col * dx - gridSize;
            float wy = row * dy - gridSize;
            if (col < cols) {
                gridLines.push_back(wx); gridLines.push_back(wy);
                gridLines.push_back(wx+dx); gridLines.push_back(wy);
            }
            if (row < rows) {
                gridLines.push_back(wx); gridLines.push_back(wy);
                gridLines.push_back(wx); gridLines.push_back(wy+dy);
            }
        }
    }
    glGenVertexArrays(1, &gridVAO); glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO); glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(float), gridLines.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Ensure we start with exactly the same fast physics simulation parameters
    globalSim = new Simulation(0.01);
    globalSim->addBody({"Sun",      {0,   0,   0}, {0,    0,   0}, 1000.0, 50.0});
    globalSim->addBody({"Mercury",  {-120, 0,  0}, {0,  2.8,   0},   2.0,   8.0});
    globalSim->addBody({"Venus",    {200, 0,  0}, {0,  -2.2,   0},   15.0, 18.0});
    globalSim->addBody({"Earth",    {-300, 20, 0}, {0,   -2.2, 0},   20.0, 24.0});
    globalSim->addBody({"Mars",     {400, 50, 0}, {-0.2, 1.5, 0},    8.0, 14.0});
    globalSim->addBody({"Jupiter",  {-800, -50, 0}, {0,  -1.2, 0},   150.0, 40.0});
    globalSim->addBody({"Saturn",   {1200, 100, 0}, {0,   0.9, 0},   90.0,  35.0});
    emscriptenSim = globalSim;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(emulateMainLoop, 0, 1);
#else
    while (!glfwWindowShouldClose(win)) {
        emulateMainLoop();
    }
    glfwTerminate();
#endif
    return 0;
}
