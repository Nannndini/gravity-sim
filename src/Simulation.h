#pragma once
#include "Body.h"
#include <vector>

class Simulation {
public:
    // Gravitational constant (scaled for simulation units)
    // In real units: 6.674e-11 N·m²/kg²
    // Here we use G=1.0 with scaled masses/distances
    static constexpr double G       = 1.0;
    static constexpr double EPSILON = 0.5;   // softening — avoids singularity at r=0

    explicit Simulation(double dt = 0.01);

    void addBody(Body body);
    void step();                    // one Euler step
    void stepVerlet();              // one Velocity-Verlet step (more stable)

    double totalEnergy()     const;
    double totalKE()         const;
    double totalPE()         const;
    Vec3   centerOfMass()    const;
    Vec3   totalMomentum()   const;

    const std::vector<Body>& bodies() const { return _bodies; }
    double time() const { return _time; }

private:
    std::vector<Body>   _bodies;
    std::vector<Vec3>   _prevPos;   // used by Verlet
    double              _dt;
    double              _time;

    // Compute net acceleration on body i from all others
    Vec3 acceleration(int i) const;
};
