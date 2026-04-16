#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <limits>
#include <emscripten.h>
#include <emscripten/html5.h>
 
const char* vertexShaderSource = R"glsl(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out float lightIntensity;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vec3 normal = normalize(aPos);
    vec3 worldPos = (model * vec4(aPos, 1.0)).xyz;
    vec3 dirToCenter = normalize(-worldPos);
    lightIntensity = max(dot(normal, dirToCenter), 0.15);
})glsl";
 
const char* fragmentShaderSource = R"glsl(#version 300 es
precision highp float;
in float lightIntensity;
out vec4 FragColor;
uniform vec4 objectColor;
uniform bool isGrid;
uniform bool GLOW;
void main() {
    if (isGrid) {
        FragColor = objectColor;
    } else if (GLOW) {
        FragColor = vec4(objectColor.rgb * 2.0, objectColor.a);
    } else {
        float fade = smoothstep(0.0, 1.0, lightIntensity);
        FragColor = vec4(objectColor.rgb * max(fade, 0.3), objectColor.a);
    }
})glsl";
 
bool running = true;
bool paused = false;
glm::vec3 cameraPos   = glm::vec3(0.0f, 1000.0f, 5000.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.2f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);
float yaw = -90.0f, pitch = -10.0f;
float deltaTime = 0.016f, lastFrame = 0.0f;
float lastX = 400.0f, lastY = 300.0f;
bool firstMouse = true;
 
const double G = 6.6743e-11;
const float c = 299792458.0f;
float initMass = float(pow(10, 22));
float sizeRatio = 30000.0f;
 
GLFWwindow* window;
GLuint shaderProgram;
GLuint gridVAO, gridVBO;
GLint modelLoc, objectColorLoc;
 
bool keys[512] = {};
 
glm::vec3 sphericalToCartesian(float r, float theta, float phi) {
    return glm::vec3(r*sin(theta)*cos(phi), r*cos(theta), r*sin(theta)*sin(phi));
}
 
void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}
 
struct Object {
    GLuint VAO, VBO;
    glm::vec3 position;
    glm::vec3 velocity;
    size_t vertexCount;
    glm::vec4 color;
    bool Initalizing = false;
    bool Launched = false;
    float mass, density, radius;
    bool glow;
 
    std::vector<float> makeVerts() {
        std::vector<float> verts;
        int stacks = 10, sectors = 10;
        for (float i = 0; i <= stacks; ++i) {
            float t1 = (i/stacks)*3.14159f;
            float t2 = ((i+1)/stacks)*3.14159f;
            for (float j = 0; j < sectors; ++j) {
                float p1 = (j/sectors)*2*3.14159f;
                float p2 = ((j+1)/sectors)*2*3.14159f;
                auto v1=sphericalToCartesian(radius,t1,p1);
                auto v2=sphericalToCartesian(radius,t1,p2);
                auto v3=sphericalToCartesian(radius,t2,p1);
                auto v4=sphericalToCartesian(radius,t2,p2);
                verts.insert(verts.end(),{v1.x,v1.y,v1.z});
                verts.insert(verts.end(),{v2.x,v2.y,v2.z});
                verts.insert(verts.end(),{v3.x,v3.y,v3.z});
                verts.insert(verts.end(),{v2.x,v2.y,v2.z});
                verts.insert(verts.end(),{v4.x,v4.y,v4.z});
                verts.insert(verts.end(),{v3.x,v3.y,v3.z});
            }
        }
        return verts;
    }
 
    Object(glm::vec3 pos, glm::vec3 vel, float m, float d=3344,
           glm::vec4 col=glm::vec4(1,0,0,1), bool gl=false)
        : position(pos), velocity(vel), mass(m), density(d), color(col), glow(gl)
    {
        radius = pow((3*mass/density)/(4*3.14159f), 1.0f/3.0f) / sizeRatio;
        auto v = makeVerts();
        vertexCount = v.size();
        CreateVBOVAO(VAO, VBO, v.data(), vertexCount);
    }
 
    void UpdateVertices() {
        radius = pow((3*mass/density)/(4*3.14159f), 1.0f/3.0f) / sizeRatio;
        auto v = makeVerts();
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(float), v.data(), GL_STATIC_DRAW);
    }
 
    void UpdatePos() {
        position += velocity * (deltaTime * 0.5f);
    }
 
    void accelerate(float x, float y, float z) {
        velocity += glm::vec3(x,y,z) * deltaTime;
    }
 
    float CheckCollision(const Object& other) {
        glm::vec3 d = other.position - position;
        float dist = glm::length(d);
        return (other.radius + radius > dist) ? -0.2f : 1.0f;
    }
};
 
std::vector<Object> objs;
 
std::vector<float> CreateGridVertices(float size, int divisions) {
    std::vector<float> verts;
    float step = size / divisions;
    float half = size / 2.0f;
    float y = 0.0f;
    for (int z = 0; z <= divisions; ++z) {
        float zv = -half + z*step;
        for (int x = 0; x < divisions; ++x) {
            float x1 = -half + x*step, x2 = x1+step;
            verts.insert(verts.end(), {x1,y,zv, x2,y,zv});
        }
    }
    for (int x = 0; x <= divisions; ++x) {
        float xv = -half + x*step;
        for (int z = 0; z < divisions; ++z) {
            float z1 = -half + z*step, z2 = z1+step;
            verts.insert(verts.end(), {xv,y,z1, xv,y,z2});
        }
    }
    return verts;
}
 
std::vector<float> UpdateGridVertices(std::vector<float> verts) {
    for (int i = 0; i < (int)verts.size(); i += 3) {
        glm::vec3 vp(verts[i], verts[i+1], verts[i+2]);
        float dy = 0.0f;
        for (const auto& obj : objs) {
            glm::vec3 toObj = obj.position - vp;
            float dist = glm::length(toObj);
            float dist_m = dist * 1000.0f;
            float rs = (2.0*G*obj.mass)/(double(c)*double(c));
            if (dist_m > rs && rs > 0) {
                float dz = 2.0f * sqrt(rs * (dist_m - rs));
                dy += dz * 2.0f;
            }
        }
        verts[i+1] = -fabs(dy) * 0.0003f;
    }
    return verts;
}
 
GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s,512,nullptr,log); std::cerr<<log; }
    return s;
}
 
void UpdateCam() {
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos+cameraFront, cameraUp);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"view"),1,GL_FALSE,glm::value_ptr(view));
}
 
void mainLoop() {
    float now = glfwGetTime();
    deltaTime = now - lastFrame;
    lastFrame = now;
    if (deltaTime > 0.05f) deltaTime = 0.05f;
 
    float speed = 2000.0f * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (keys[GLFW_KEY_W]) cameraPos += speed * cameraFront;
    if (keys[GLFW_KEY_S]) cameraPos -= speed * cameraFront;
    if (keys[GLFW_KEY_A]) cameraPos -= speed * right;
    if (keys[GLFW_KEY_D]) cameraPos += speed * right;
    if (keys[GLFW_KEY_SPACE]) cameraPos += speed * cameraUp;
    if (keys[GLFW_KEY_LEFT_SHIFT]) cameraPos -= speed * cameraUp;
 
    if (!objs.empty() && objs.back().Initalizing) {
        float mv = objs.back().radius * 0.3f;
        if (keys[GLFW_KEY_LEFT])  objs.back().position.x -= mv;
        if (keys[GLFW_KEY_RIGHT]) objs.back().position.x += mv;
        if (keys[GLFW_KEY_UP])    objs.back().position.z += mv;
        if (keys[GLFW_KEY_DOWN])  objs.back().position.z -= mv;
    }
 
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);
    UpdateCam();
 
    // draw grid
    auto gv = UpdateGridVertices(CreateGridVertices(20000.0f, 25));
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gv.size()*sizeof(float), gv.data(), GL_DYNAMIC_DRAW);
    glm::mat4 identity = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLoc,1,GL_FALSE,glm::value_ptr(identity));
    glUniform4f(objectColorLoc, 1,1,1,0.25f);
    glUniform1i(glGetUniformLocation(shaderProgram,"isGrid"),1);
    glUniform1i(glGetUniformLocation(shaderProgram,"GLOW"),0);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gv.size()/3);
    glBindVertexArray(0);
 
    // physics + draw bodies
    int n = objs.size();
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            if (objs[i].Initalizing || objs[j].Initalizing) continue;
            glm::vec3 d = objs[j].position - objs[i].position;
            float dist = glm::length(d);
            if (dist > 0) {
                float dist_m = dist * 1000.0f;
                double Gf = (G * objs[i].mass * objs[j].mass) / (double(dist_m)*double(dist_m));
                glm::vec3 dir = glm::normalize(d);
                float a1 = float(Gf / objs[i].mass);
                float a2 = float(Gf / objs[j].mass);
                if (!paused) {
                    objs[i].accelerate(dir.x*a1, dir.y*a1, dir.z*a1);
                    objs[j].accelerate(-dir.x*a2, -dir.y*a2, -dir.z*a2);
                }
                objs[i].velocity *= objs[i].CheckCollision(objs[j]);
            }
        }
        if (!paused) objs[i].UpdatePos();
 
        glm::mat4 model = glm::translate(glm::mat4(1.0f), objs[i].position);
        glUniformMatrix4fv(modelLoc,1,GL_FALSE,glm::value_ptr(model));
        glUniform4f(objectColorLoc, objs[i].color.r, objs[i].color.g, objs[i].color.b, objs[i].color.a);
        glUniform1i(glGetUniformLocation(shaderProgram,"isGrid"),0);
        glUniform1i(glGetUniformLocation(shaderProgram,"GLOW"), objs[i].glow ? 1 : 0);
        glBindVertexArray(objs[i].VAO);
        glDrawArrays(GL_TRIANGLES, 0, objs[i].vertexCount/3);
        glBindVertexArray(0);
    }
 
    glfwSwapBuffers(window);
    glfwPollEvents();
}
 
void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (key >= 0 && key < 512) {
        if (action == GLFW_PRESS) keys[key] = true;
        if (action == GLFW_RELEASE) keys[key] = false;
    }
    if (key == GLFW_KEY_K && action == GLFW_PRESS) paused = !paused;
    if (key == GLFW_KEY_M && action == GLFW_PRESS) initMass *= 2.0f;
    if (key == GLFW_KEY_R && action == GLFW_PRESS) objs.clear();
}
 
void spawnPlanet(float x, float z) {
    std::vector<glm::vec4> colors = {
        {0,1,1,1},{1,0.93f,0.18f,1},{1,0.3f,0.3f,1},{0.5f,1,0.5f,1},{1,0.5f,1,1}
    };
    static int ci = 0;
    glm::vec4 col = colors[ci++ % colors.size()];
    objs.emplace_back(glm::vec3(x, 650, z), glm::vec3(0,0,0), initMass, 5515, col, false);
}
 
void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        spawnPlanet(cameraPos.x, cameraPos.z - 2000);
    }
}
 
void spaceCallback(GLFWwindow* w, int key, int, int action, int mods) {
    keyCallback(w, key, 0, action, mods);
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        spawnPlanet(cameraPos.x, cameraPos.z - 2000);
    }
}
 
void mouseCallback(GLFWwindow* w, double xpos, double ypos) {
    if (firstMouse) { lastX=xpos; lastY=ypos; firstMouse=false; }
    float xo = (xpos-lastX)*0.1f;
    float yo = (lastY-ypos)*0.1f;
    lastX=xpos; lastY=ypos;
    yaw+=xo; pitch+=yo;
    if(pitch>89) pitch=89;
    if(pitch<-89) pitch=-89;
    cameraFront = glm::normalize(glm::vec3(
        cos(glm::radians(yaw))*cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw))*cos(glm::radians(pitch))
    ));
}
 
void scrollCallback(GLFWwindow*, double, double yo) {
    float s = 500000.0f * deltaTime;
    cameraPos += float(yo) * s * cameraFront;
}
 
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    window = glfwCreateWindow(800, 600, "Gravity Sandbox", nullptr, nullptr);
    glfwMakeContextCurrent(window);
 
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
 
    modelLoc = glGetUniformLocation(shaderProgram, "model");
    objectColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
 
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 800.0f/600.0f, 0.1f, 750000.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram,"projection"),1,GL_FALSE,glm::value_ptr(proj));
 
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 
    auto gv = CreateGridVertices(20000.0f, 25);
    CreateVBOVAO(gridVAO, gridVBO, gv.data(), gv.size());
 
    // starting bodies
    objs.emplace_back(glm::vec3(-5000,650,-350), glm::vec3(0,0,1500), 5.97e22f, 5515, glm::vec4(0,1,1,1));
    objs.emplace_back(glm::vec3( 5000,650,-350), glm::vec3(0,0,-1500), 5.97e22f, 5515, glm::vec4(0,1,1,1));
    objs.emplace_back(glm::vec3(0,0,-350), glm::vec3(0,0,0), 1.989e25f, 5515, glm::vec4(1,0.93f,0.18f,1), true);
 
    glfwSetKeyCallback(window, spaceCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
 
    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}
