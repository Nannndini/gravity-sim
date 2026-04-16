#pragma once
#include <string>
#include <deque>

struct Vec3 {
    double x, y, z;

    Vec3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(double s)      const { return {x*s,   y*s,   z*s};   }
    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }

    double dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    double lengthSq()         const { return dot(*this); }
    double length()           const;
    Vec3   normalized()       const;
};

struct Body {
    std::string name;
    Vec3   pos;
    Vec3   vel;
    double mass;
    double radius;
    std::deque<Vec3> trail;

    Body(std::string name, Vec3 pos, Vec3 vel, double mass, double radius)
        : name(std::move(name)), pos(pos), vel(vel), mass(mass), radius(radius) {}

    double kineticEnergy()  const;
    double speed()          const;
};
