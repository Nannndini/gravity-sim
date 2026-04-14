#include "Simulation.h"
#include <cmath>
#include <stdexcept>

Simulation::Simulation(double dt) : _dt(dt), _time(0.0) {}

void Simulation::addBody(Body body) {
    _prevPos.push_back(body.pos);
    _bodies.push_back(std::move(body));
}

// Compute gravitational acceleration on body i from all other bodies
// a_i = sum_j [ G * m_j * (r_j - r_i) / (|r_j - r_i|^2 + eps^2)^(3/2) ]
Vec3 Simulation::acceleration(int i) const {
    Vec3 acc;
    const Vec3& pi = _bodies[i].pos;

    for (int j = 0; j < (int)_bodies.size(); ++j) {
        if (i == j) continue;

        Vec3 diff = _bodies[j].pos - pi;                     // vector from i → j
        double r2  = diff.lengthSq() + EPSILON * EPSILON;    // softened distance²
        double r   = std::sqrt(r2);
        double mag  = G * _bodies[j].mass / r2;              // |a| = G*m/r²

        acc += diff.normalized() * mag;
    }
    return acc;
}

// Basic Euler integration — simple but loses energy over time
void Simulation::step() {
    int n = _bodies.size();
    std::vector<Vec3> accs(n);

    // 1. Compute all accelerations first (before moving anything)
    for (int i = 0; i < n; ++i)
        accs[i] = acceleration(i);

    // 2. Update velocities and positions
    for (int i = 0; i < n; ++i) {
        _bodies[i].vel += accs[i] * _dt;
        _bodies[i].pos += _bodies[i].vel * _dt;
    }

    _time += _dt;
}

// Velocity-Verlet — much better energy conservation for orbits
// Algorithm:
//   1. pos(t+dt)  = pos(t) + vel(t)*dt + 0.5*a(t)*dt²
//   2. a(t+dt)    = f( pos(t+dt) )
//   3. vel(t+dt)  = vel(t) + 0.5*(a(t) + a(t+dt))*dt
void Simulation::stepVerlet() {
    int n = _bodies.size();
    std::vector<Vec3> oldAcc(n);
    std::vector<Vec3> newAcc(n);

    // Step 1: compute current accelerations
    for (int i = 0; i < n; ++i)
        oldAcc[i] = acceleration(i);

    // Step 2: update positions
    for (int i = 0; i < n; ++i) {
        _bodies[i].pos += _bodies[i].vel * _dt
                        + oldAcc[i] * (0.5 * _dt * _dt);
    }

    // Step 3: compute new accelerations at updated positions
    for (int i = 0; i < n; ++i)
        newAcc[i] = acceleration(i);

    // Step 4: update velocities using average acceleration
    for (int i = 0; i < n; ++i) {
        _bodies[i].vel += (oldAcc[i] + newAcc[i]) * (0.5 * _dt);
    }

    _time += _dt;
}

double Simulation::totalKE() const {
    double ke = 0;
    for (const auto& b : _bodies) ke += b.kineticEnergy();
    return ke;
}

double Simulation::totalPE() const {
    double pe = 0;
    int n = _bodies.size();
    for (int i = 0; i < n; ++i)
        for (int j = i+1; j < n; ++j) {
            Vec3 diff = _bodies[j].pos - _bodies[i].pos;
            double r = std::sqrt(diff.lengthSq() + EPSILON * EPSILON);
            pe -= G * _bodies[i].mass * _bodies[j].mass / r;
        }
    return pe;
}

double Simulation::totalEnergy() const {
    return totalKE() + totalPE();
}

Vec3 Simulation::centerOfMass() const {
    Vec3 com;
    double totalMass = 0;
    for (const auto& b : _bodies) {
        com  += b.pos * b.mass;
        totalMass += b.mass;
    }
    if (totalMass > 0) return com * (1.0 / totalMass);
    return com;
}

Vec3 Simulation::totalMomentum() const {
    Vec3 p;
    for (const auto& b : _bodies) p += b.vel * b.mass;
    return p;
}
