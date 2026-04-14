#include "Simulation.h"
#include <iostream>
#include <iomanip>
#include <cmath>

// ── Scene setup ──────────────────────────────────────────────────────────────
// Units: distance in AU-equivalents, mass in solar-mass-equivalents, G = 1
// Circular orbit speed: v = sqrt(G * M / r)

void setupSolarSystem(Simulation& sim) {
    // Sun at center, nearly at rest
    sim.addBody({"Sun",   {0,   0, 0}, {0,      0,    0}, 1000.0, 5.0});

    // Earth-like planet at r=100 → v = sqrt(1000/100) ≈ 3.16
    sim.addBody({"Earth", {100, 0, 0}, {0,      3.16, 0},    1.0, 1.5});

    // Mars-like planet at r=150 → v = sqrt(1000/150) ≈ 2.58
    sim.addBody({"Mars",  {150, 0, 0}, {0,      2.58, 0},    0.5, 1.2});

    // Binary companion to Earth (moon-like), offset slightly
    sim.addBody({"Moon",  {103, 0, 0}, {0,      3.90, 0},    0.01, 0.5});
}

void setupFigureEight(Simulation& sim) {
    // Famous figure-8 three-body choreography
    // Chenciner & Montgomery (2000) — specific initial conditions
    double m = 1.0;
    sim.addBody({"A", { 0.9700436, -0.2430870, 0},
                      { 0.4662036,  0.4323657, 0}, m, 1.0});
    sim.addBody({"B", {-0.9700436,  0.2430870, 0},
                      { 0.4662036,  0.4323657, 0}, m, 1.0});
    sim.addBody({"C", { 0.0,        0.0,       0},
                      {-0.9324072, -0.8647315, 0}, m, 1.0});
}

// ── Printing ─────────────────────────────────────────────────────────────────
void printHeader(const Simulation& sim) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n=== Gravity Simulation (Velocity-Verlet) ===\n";
    std::cout << "Bodies: " << sim.bodies().size() << "\n";
    std::cout << "G = " << Simulation::G
              << "  eps = " << Simulation::EPSILON << "\n\n";
}

void printState(const Simulation& sim, int step) {
    std::cout << "t=" << std::setw(8) << sim.time()
              << "  E=" << std::setw(10) << sim.totalEnergy()
              << "  KE=" << std::setw(9) << sim.totalKE()
              << "  PE=" << std::setw(10) << sim.totalPE()
              << "\n";

    for (const auto& b : sim.bodies()) {
        std::cout << "  " << std::setw(6) << b.name
                  << "  pos=(" << std::setw(8) << b.pos.x
                  << ", "      << std::setw(8) << b.pos.y << ")"
                  << "  speed=" << std::setw(7) << b.speed()
                  << "\n";
    }
    std::cout << "\n";
}

// ── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    bool figureEight = (argc > 1 && std::string(argv[1]) == "--figure8");

    Simulation sim(0.01);

    if (figureEight) {
        std::cout << "Running: figure-8 three-body choreography\n";
        setupFigureEight(sim);
    } else {
        std::cout << "Running: solar system (use --figure8 for other scene)\n";
        setupSolarSystem(sim);
    }

    printHeader(sim);

    double initialEnergy = sim.totalEnergy();
    int    printEvery    = 100;   // print every N steps
    int    totalSteps    = 2000;

    for (int step = 0; step <= totalSteps; ++step) {
        if (step % printEvery == 0) {
            printState(sim, step);

            // Energy drift check — measures integration accuracy
            double drift = std::abs((sim.totalEnergy() - initialEnergy) / initialEnergy) * 100.0;
            std::cout << "  [energy drift: " << drift << "%]\n\n";
        }
        sim.stepVerlet();
    }

    std::cout << "Done. Final time: " << sim.time() << "\n";
    return 0;
}
