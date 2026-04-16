# Gravity Sim

A real-time N-body gravitational simulation in C++.

Inspired by [kavan's "Simulating Gravity in C++"](https://www.youtube.com/watch?v=...) — built from scratch as a learning project.

## What it does

- Simulates gravitational attraction between N bodies using Newton's law: `F = G·m₁·m₂/r²`
- Two integration methods: **Euler** (simple) and **Velocity-Verlet** (stable, conserves energy)
- Softening factor `ε` prevents singularities when bodies overlap
- Tracks total energy (KE + PE) to measure simulation accuracy
- Scenes: solar system layout and the famous figure-8 three-body choreography

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Run

```bash
# Solar system scene
./gravity_sim

# Figure-8 three-body choreography
./gravity_sim --figure8
```

## Physics

```
acceleration_i = Σⱼ [ G·mⱼ·(rⱼ - rᵢ) / (|rⱼ - rᵢ|² + ε²)^(3/2) ]
```

Velocity-Verlet integration (used by default):
```
pos(t+dt) = pos(t) + vel(t)·dt + ½·a(t)·dt²
vel(t+dt) = vel(t) + ½·(a(t) + a(t+dt))·dt
```

## Roadmap

- [x] v0.1 — Physics engine (terminal output)
- [x] v0.2 — OpenGL window, render bodies as spheres
- [x] v0.3 — Spacetime grid mesh (warped by mass)
- [ ] v0.4 — Collision detection and merging

## Structure

```
gravity-sim/
├── src/
│   ├── Body.h / Body.cpp         — Vec3 math + Body struct
│   ├── Simulation.h / Simulation.cpp — physics engine
│   └── main.cpp                  — scenes + entry point
├── CMakeLists.txt
└── README.md
```