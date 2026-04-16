#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>

// ── Minimal physics (same as before) ─────────────────────────────────────────
struct Vec2 { double x, y; };

struct Body {
    std::string name;
    Vec2   pos, vel;
    double mass, radius;
    float  r, g, b;   // color
    std::vector<Vec2> trail;
};

static const double G       = 1.0;
static const double EPSILON = 0.5;
static const double DT      = 0.01;
static const int    TRAIL   = 200;

std::vector<Body> bodies;

Vec2 accel(int i) {
    Vec2 a = {0, 0};
    for (int j = 0; j < (int)bodies.size(); ++j) {
        if (i == j) continue;
        double dx = bodies[j].pos.x - bodies[i].pos.x;
        double dy = bodies[j].pos.y - bodies[i].pos.y;
        double r2 = dx*dx + dy*dy + EPSILON*EPSILON;
        double r  = sqrt(r2);
        double f  = G * bodies[j].mass / r2;
        a.x += f * dx / r;
        a.y += f * dy / r;
    }
    return a;
}

void stepVerlet() {
    int n = bodies.size();
    std::vector<Vec2> oldA(n), newA(n);
    for (int i = 0; i < n; ++i) oldA[i] = accel(i);
    for (int i = 0; i < n; ++i) {
        bodies[i].pos.x += bodies[i].vel.x * DT + 0.5 * oldA[i].x * DT * DT;
        bodies[i].pos.y += bodies[i].vel.y * DT + 0.5 * oldA[i].y * DT * DT;
    }
    for (int i = 0; i < n; ++i) newA[i] = accel(i);
    for (int i = 0; i < n; ++i) {
        bodies[i].vel.x += 0.5 * (oldA[i].x + newA[i].x) * DT;
        bodies[i].vel.y += 0.5 * (oldA[i].y + newA[i].y) * DT;
        bodies[i].trail.push_back(bodies[i].pos);
        if ((int)bodies[i].trail.size() > TRAIL)
            bodies[i].trail.erase(bodies[i].trail.begin());
    }
}

void setupScene() {
    // Sun
    bodies.push_back({"Sun",   {0,0},{0,0},       1000.0, 18.0, 1.0f, 0.9f, 0.2f, {}});
    // Earth
    bodies.push_back({"Earth", {200,0},{0,3.16},    1.0,  6.0, 0.2f, 0.5f, 1.0f, {}});
    // Mars
    bodies.push_back({"Mars",  {300,0},{0,2.58},    0.5,  5.0, 0.9f, 0.3f, 0.1f, {}});
    // Moon (orbiting Earth)
    bodies.push_back({"Moon",  {206,0},{0,3.90},    0.01, 3.0, 0.8f, 0.8f, 0.8f, {}});
    // Jupiter
    bodies.push_back({"Jupiter",{420,0},{0,2.18},   5.0,  10.0,0.8f, 0.6f, 0.3f, {}});
}

// ── OpenGL helpers ────────────────────────────────────────────────────────────
void drawCircle(float cx, float cy, float r, float red, float grn, float blu, float alpha=1.0f) {
    glColor4f(red, grn, blu, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    int segs = 40;
    for (int i = 0; i <= segs; ++i) {
        float a = i * 2.0f * 3.14159f / segs;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

void drawTrail(const Body& b) {
    int n = b.trail.size();
    if (n < 2) return;
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < n; ++i) {
        float alpha = (float)i / n;
        glColor4f(b.r, b.g, b.b, alpha * 0.6f);
        glVertex2f((float)b.trail[i].x, (float)b.trail[i].y);
    }
    glEnd();
}

// ── Spacetime grid ────────────────────────────────────────────────────────────
void drawGrid(int W, int H) {
    int cols = 30, rows = 30;
    float dx = (float)W / cols;
    float dy = (float)H / rows;

    glLineWidth(0.5f);
    glBegin(GL_LINES);
    for (int row = 0; row <= rows; ++row) {
        for (int col = 0; col <= cols; ++col) {
            // World position of this grid point
            float wx = col * dx - W/2.0f;
            float wy = row * dy - H/2.0f;

            // Warp: pull grid points toward massive bodies
            float warpX = 0, warpY = 0;
            for (const auto& b : bodies) {
                float ddx = (float)b.pos.x - wx;
                float ddy = (float)b.pos.y - wy;
                float dist2 = ddx*ddx + ddy*ddy + 1.0f;
                float strength = (float)b.mass * 800.0f / dist2;
                strength = fminf(strength, 60.0f);
                warpX += strength * ddx / sqrtf(dist2);
                warpY += strength * ddy / sqrtf(dist2);
            }

            float px = wx + warpX;
            float py = wy + warpY;

            // Draw horizontal segment to next col
            if (col < cols) {
                float wx2 = (col+1)*dx - W/2.0f;
                float wy2 = row*dy    - H/2.0f;
                float warpX2=0, warpY2=0;
                for (const auto& b : bodies) {
                    float ddx=(float)b.pos.x-wx2, ddy=(float)b.pos.y-wy2;
                    float d2=ddx*ddx+ddy*ddy+1.0f;
                    float s=fminf((float)b.mass*800.0f/d2,60.0f);
                    warpX2+=s*ddx/sqrtf(d2); warpY2+=s*ddy/sqrtf(d2);
                }
                glColor4f(0.3f, 0.2f, 0.6f, 0.4f);
                glVertex2f(px, py);
                glVertex2f(wx2+warpX2, wy2+warpY2);
            }
            // Draw vertical segment to next row
            if (row < rows) {
                float wx2 = col*dx    - W/2.0f;
                float wy2 = (row+1)*dy- H/2.0f;
                float warpX2=0, warpY2=0;
                for (const auto& b : bodies) {
                    float ddx=(float)b.pos.x-wx2, ddy=(float)b.pos.y-wy2;
                    float d2=ddx*ddx+ddy*ddy+1.0f;
                    float s=fminf((float)b.mass*800.0f/d2,60.0f);
                    warpX2+=s*ddx/sqrtf(d2); warpY2+=s*ddy/sqrtf(d2);
                }
                glColor4f(0.3f, 0.2f, 0.6f, 0.4f);
                glVertex2f(px, py);
                glVertex2f(wx2+warpX2, wy2+warpY2);
            }
        }
    }
    glEnd();
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

    int W = 1000, H = 800;
    GLFWwindow* win = glfwCreateWindow(W, H, "Gravity Sim v0.2", nullptr, nullptr);
    if (!win) { std::cerr << "Window creation failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n"; return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    setupScene();

    while (!glfwWindowShouldClose(win)) {
        // Physics: multiple steps per frame for smooth animation
        for (int i = 0; i < 5; ++i) stepVerlet();

        glfwGetFramebufferSize(win, &W, &H);
        glViewport(0, 0, W, H);

        // Dark background
        glClearColor(0.03f, 0.02f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Orthographic projection centered at origin
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)W / H;
        float viewH = 550.0f;
        float viewW = viewH * aspect;
        glOrtho(-viewW, viewW, -viewH, viewH, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Draw spacetime grid
        drawGrid(W, H);

        // Draw trails
        for (const auto& b : bodies) drawTrail(b);

        // Draw glow (larger, semi-transparent circle behind body)
        for (const auto& b : bodies)
            drawCircle((float)b.pos.x, (float)b.pos.y,
                       (float)b.radius * 2.2f,
                       b.r, b.g, b.b, 0.15f);

        // Draw bodies
        for (const auto& b : bodies)
            drawCircle((float)b.pos.x, (float)b.pos.y,
                       (float)b.radius,
                       b.r, b.g, b.b, 1.0f);

        glfwSwapBuffers(win);
        glfwPollEvents();

        // ESC to quit
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(win, true);
    }

    glfwTerminate();
    return 0;
}