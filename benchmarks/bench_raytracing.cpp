#include <random>

#include <benchmark/benchmark.h>

#include <psac/psac.hpp>
#include <psac/examples/raytracer.hpp>

const auto max_n = 1;
#include "common.hpp"

constexpr int SCENE_SIZE = 2000;

namespace static_raytracer {

enum class SurfaceType {
  Shiny,
  Matte
};

struct Object {
  virtual ~Object() = default;
  
  virtual std::optional<std::pair<Vector, double>> intersect(const Ray& ray) const = 0;
  virtual Vector normal(const Vector& pos) const = 0;
  virtual void get_colour (const Vector pt, Colour* c) = 0;
  virtual double reflectivity() const = 0;
};

struct Ball : public Object {
  Ball(Vector _center, double _r, SurfaceType _type, Colour _colour)
    : center(_center), radius(_r), type(_type) {
    
    colour = _colour;
  }

  // Compute the intersection of the given ray with this sphere. Returns
  // none if there is no such intersection.
  virtual std::optional<std::pair<Vector, double>> intersect(const Ray& ray) const override {
    auto eo = center - ray.head;
    double v = eo.dot(ray.dir);
    double dist = 0;
    if (v < 0) {
      dist = 0;
    }
    else {
      double disc = pow(radius, 2) - (eo.norm2() - pow(v, 2));
      dist = disc < 0 ? 0 : v - sqrt(disc);
    }
    if (dist == 0) return {};
    else return std::make_pair(ray.pos(dist), dist);
  }
  
  // Return a unit normal vector to the sphere at the given point
  virtual Vector normal(const Vector& pos) const override {
    return (pos - center).normalize();
  }

  virtual void get_colour(const Vector, Colour* c) override {
    *c = colour;
  }

  virtual double reflectivity() const override {
    if (type == SurfaceType::Shiny) return 0.3;
    else return 0.0;
  }

  void change_colour(const Colour& new_col) {
    colour = new_col;
  }

  Vector center;
  double radius;
  SurfaceType type;
  Colour colour;
};

struct Plane : public Object {
  Plane(Vector _normal, Vector _center, Colour _colour) : normal_dir(_normal.normalize()), center(_center) {
    colour = _colour;
  }

  virtual std::optional<std::pair<Vector, double>> intersect(const Ray& ray) const override {
    double denom = normal_dir.dot(ray.dir);
    if (denom >= 0) return {};
    else {
      double proj = (ray.head - center).dot(normal_dir);
      if (proj < 0) return {};
      double d = proj / (-denom);
      return std::make_pair(ray.pos(d), d);
    }
  }

  virtual Vector normal(const Vector&) const override {
    return normal_dir;
  }
  
  virtual void get_colour(const Vector, Colour* c) override {
    *c = colour;
  }

  virtual double reflectivity() const override {
    return 0.0;
  }

  Vector normal_dir;
  Vector center;
  Colour colour;
};

// ------------------------------------------------------------------
//                         Lights and Scene
// ------------------------------------------------------------------


struct Scene {
  Scene() = default;
  Scene(Scene&&) = default;

  void add_light(Light light) {
    lights.push_back(light);
  }

  void add_object(std::unique_ptr<Object> object) {
    objects.push_back(std::move(object));
  }

  static std::pair<Scene, Ball*> default_scene() {
    // Walls 
    auto floor = std::make_unique<Plane>(Vector(0,0,1), Vector(0,0,-1), Colour(0.1,0.2,0.3));
    auto wall1 = std::make_unique<Plane>(Vector(1,-1,0), Vector(0,10,0), Colour(0.3,0.2,0.1));
    auto wall2 = std::make_unique<Plane>(Vector(-1,-1,0), Vector(0,10,0), Colour(0.3,0.2,0.1));
    auto wall3 = std::make_unique<Plane>(Vector(0,0,-1), Vector(0,0,20), Colour(0.3,0.2,0.1));
    auto wall4 = std::make_unique<Plane>(Vector(0,1,0), Vector(0,-20,0), Colour(0.3,0.2,0.1));

    // Balls
    auto ball1 = std::make_unique<Ball>(Vector(0,5,0), 1, SurfaceType::Shiny, Colour(1,0,0));
    auto ball2 = std::make_unique<Ball>(Vector(-2,2,-0.5), 0.5, SurfaceType::Shiny, Colour(0,1,0));
    auto ball3 = std::make_unique<Ball>(Vector(2,1,0.5), 1.5, SurfaceType::Shiny, Colour(0,0,1));
    Ball* ball_ptr = ball2.get();

    // Lights
    auto light1 = Light(Vector(-1, 5, 1), 1, 1.5);

    Scene scene;
    scene.add_object(std::move(floor));
    scene.add_object(std::move(wall1));
    scene.add_object(std::move(wall2));
    scene.add_object(std::move(wall3));
    scene.add_object(std::move(wall4));

    scene.add_object(std::move(ball1));
    scene.add_object(std::move(ball2));
    scene.add_object(std::move(ball3));

    scene.add_light(light1);

    return std::make_pair(std::move(scene), ball_ptr);
  }

  std::vector<Light> lights;
  std::vector<std::unique_ptr<Object>> objects;
};

// ------------------------------------------------------------------
//                           Ray tracing
// ------------------------------------------------------------------

struct RayTracer {
  RayTracer(int _width, int _height, int _maxd, double _base_bright, Scene& _scene, Camera& _camera)
    : width(_width), height(_height), max_depth(_maxd),
    base_brightness(_base_bright), image(width, std::vector<Colour>(height)), scene(_scene),
    camera(_camera) { }

  std::optional<std::pair<Object*, Vector>> shoot_ray(const Ray& ray) {
    double closest_dist = 1e100;
    Object* closest_object = nullptr;
    Vector closest_point;
    for (const auto& object : scene.objects) {
      auto inter = object->intersect(ray);
      if (inter) {
        double dist = inter->second;
        if (dist < closest_dist) {
          closest_dist = dist;
          closest_object = object.get();
          closest_point = inter->first;
        }
      }
    }
    if (closest_dist == 1e100) return {};
    else return std::make_pair(closest_object, closest_point);
  }

  void render(int x, int y) {
    Ray ray = camera.get_ray(x, y, width, height);
    auto bounce = shoot_ray(ray);
    if (!bounce) {
      image[x][y] = Colour::BLACK();
    }
    else {
      Colour c;
      bounce->first->get_colour(bounce->second, &c);
      auto bright = brightness(bounce->second);

      // Reflection
      double reflectivity = bounce->first->reflectivity();
      if (max_depth > 0 && reflectivity > 0.0) {
        Colour c2;
        calculate_reflective_colour(bounce->first, bounce->second, ray.dir, 1, &c2);
        image[x][y] = (1.0 - reflectivity) * (bright * c) + reflectivity * c2;
      }
      else {
        image[x][y] = bright * c;
      }
    }
  }

  void calculate_reflective_colour(const Object* o, const Vector pt, const Vector& dir,
      int depth, Colour* c) {
    Vector normal = o->normal(pt);
    Vector reflected_dir =  dir - ((2.0 * (dir.dot(normal))) * normal);
    Ray reflected(pt + 1e-1 * reflected_dir, reflected_dir);
    auto bounce = shoot_ray(reflected);
    if (!bounce) {
      *c = Colour::BLACK();
    }
    else {
      Colour c2;
      bounce->first->get_colour(bounce->second, &c2);
      auto bright = brightness(bounce->second);

      // More reflection
      if (depth < max_depth) {
        double reflectivity = bounce->first->reflectivity();
        if (reflectivity > 0.0) {
          Colour c3;
          calculate_reflective_colour(bounce->first, bounce->second, reflected.dir, depth + 1, &c3);
          *c = ((1.0 - reflectivity) * (bright * c2)) + reflectivity * c3;
        }
        else {
          *c = bright * c2;
        }
      }
      else {
        *c = bright * c2;
      }
    }
  }

  double brightness(const Vector& pt) {
    // Lighting
    double brightness = base_brightness;
    for (const auto& light : scene.lights) {
      Ray light_ray(light.pos, pt - light.pos);
      auto light_hit = shoot_ray(light_ray);
      if (light_hit && light_hit->second == pt) {
        double dist = (pt - light.pos).norm();
        brightness += light.intensity / pow(dist / light.range, 2);
      }
    }
    return brightness;
  }

  void render_seq() {
    for (int p = 0; p < width * height; p++) {
      int x = p % height;
      int y = p / height;
      render(x, y);
    }
  }

  void render_par() {
    parlay::parallel_for(0, width * height, [&](auto p) {
      int x = p % height;
      int y = p / height;
      render(x, y);
    });
  }

  void output(const std::string& filename) {
    std::ofstream out(filename);
    out << width << ' ' << height << '\n';
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        out << image[x][y].r << ' ' << image[x][y].g << ' ' << image[x][y].b << ' ';
      }
      out << '\n';
    }
  }

  int width, height;
  int max_depth;
  double base_brightness;
  std::vector<std::vector<Colour>> image;
  Scene& scene;
  Camera& camera;
};

}  // namespace static_raytracer

// Sequential baseline for raytracing
static void raytrace_compute_seq(benchmark::State& state) {
  parlay::set_num_workers(1);
  auto [scene, ball_ptr] = static_raytracer::Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  static_raytracer::RayTracer tracer(SCENE_SIZE, SCENE_SIZE, 3, 0.3, scene, camera);

  for (auto _ : state) {
    tracer.render_seq();
  }
}

// Parallel static baseline for raytracing
static void raytrace_compute_par(benchmark::State& state) {
  parlay::set_num_workers(state.range(0));
  auto [scene, ball_ptr] = static_raytracer::Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  static_raytracer::RayTracer tracer(SCENE_SIZE, SCENE_SIZE, 3, 0.3, scene, camera);
  
  for (auto _ : state) {
    tracer.render_par();
  }
}

// Parallel self-adjusting benchmark for raytracing
PSAC_STATIC_BENCHMARK(raytrace_compute)(benchmark::State& state) {

  auto [scene, ball_ptr] = Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  RayTracer tracer(SCENE_SIZE, SCENE_SIZE, 3, 0.3, scene, camera);

  // Benchmarks
  for (auto _ : state) {
    tracer.go();
    comp = std::move(tracer.comp);
    record_stats(state);
  }
}

PSAC_DYNAMIC_BENCHMARK(raytrace_update)(benchmark::State& state) { 
  
  auto [scene, ball_ptr] = Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  RayTracer tracer(SCENE_SIZE, SCENE_SIZE, 3, 0.3, scene, camera);
  tracer.go();

  std::uniform_real_distribution<double> dist(0,1);
  std::default_random_engine re;
  auto random_colour = [&]() { return Colour(dist(re), dist(re), dist(re)); };

  for (auto _ : state) {
    // Change ball to a random colour
    ball_ptr->change_colour(random_colour());

    // Propagate changes
    tracer.update();
    record_stats(state);
  }

  comp = std::move(tracer.comp);
  finalize(state);
}

REGISTER_STATIC_BENCHMARK(raytrace_compute);
REGISTER_DYNAMIC_BENCHMARK(raytrace_update);

