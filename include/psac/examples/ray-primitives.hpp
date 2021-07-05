
#ifndef PSAC_EXAMPLES_RAY_PRIMITIVES_HPP_
#define PSAC_EXAMPLES_RAY_PRIMITIVES_HPP_

#include <cmath>

// ------------------------------------------------------------------
//                            Primitives
// ------------------------------------------------------------------

struct Vector {
  Vector() = default;

  Vector(double _x, double _y, double _z)
    : x(_x), y(_y), z(_z) { }

  Vector operator+(const Vector& v) const {
    return Vector(x + v.x, y + v.y, z + v.z);
  }
  
  Vector operator-(const Vector& v) const {
    return Vector(x - v.x, y - v.y, z - v.z);
  }
  
  Vector operator*(double s) const {
    return Vector(s*x, s*y, s*z);
  }
  
  Vector cross(const Vector& v) const {
    return Vector(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x);
  }
  
  double dot(const Vector& v) const {
    return x * v.x + y * v.y + z * v.z;
  }
  
  Vector normalize() const {
    auto l = norm();
    return Vector(x / l, y / l, z / l);
  }
  
  double norm() const {
    return std::sqrt(norm2());
  }
  
  double norm2() const {
    return dot(*this);
  }
  
  double x, y, z;
};

bool operator==(const Vector& u, const Vector& v) {
  return (u - v).norm() < 1e-6;
}

Vector operator*(double s, const Vector& v) {
  return Vector(s*v.x, s*v.y, s*v.z);
}

struct Ray {
  Ray(Vector _head, Vector _dir) : head(_head), dir(_dir.normalize()) { }
  Vector head, dir;
  Vector pos(double dist) const {
    return head + dist * dir;
  }
};

struct alignas(64) Colour {
  static Colour BLACK() { return Colour(0,0,0); }

  Colour() = default;
  Colour(double _r, double _g, double _b) : r(_r), g(_g), b(_b) { }

  Colour operator+(const Colour& other) {
    return Colour(std::min(1.0, r + other.r), std::min(1.0, g + other.g), std::min(1.0, b + other.b));
  }

  bool operator!=(const Colour& other) {
    return r != other.r || g != other.g || b != other.b;
  }

  double r, g, b;
};

Colour operator*(double t, const Colour& c) {
  return Colour(std::min(1.0, t*c.r), std::min(1.0, t*c.g), std::min(1.0, t*c.b));
}

struct Light {
  Light(Vector _pos, double _intensity, double _range)
    : pos(_pos), intensity(_intensity), range(_range) { }

  Vector pos;
  double intensity;
  double range;
};

struct Camera {
  Camera(Vector _pos, Vector _dir, Vector _up) : pos(_pos), forward(_dir.normalize()), up(_up.normalize()) {
    right = forward.cross(up).normalize(); 
  }
  
  // Get the ray that points at pixel (x,y) in a screen of size (width, height)
  Ray get_ray(double x, double y, double width, double height) {
    double rx = (x - (width / 2.0)) / (2.0 * width);
    double ry = -(y - (height / 2.0)) / (2.0 * height);
    return Ray(pos, (forward + rx * right + ry * up).normalize());
  }
  
  Vector pos;
  Vector forward;
  Vector up;
  Vector right;
};

#endif  // PSAC_EXAMPLES_RAY_PRIMITIVES_HPP_

