#include <psac/psac.hpp>
#include <psac/examples/raytracer.hpp>

int main() {
  auto [scene, ball_ptr] = Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  RayTracer tracer(500, 500, 3, 0.3, scene, camera);
  std::cout << "Rendering initial scene..." << std::endl;
  tracer.go();
  std::cout << "Done!" << std::endl;
  tracer.output("raytracer-output1.txt");

  // Change the colour of the ball and update
  ball_ptr->change_colour(Colour(1, 0, 1));
  std::cout << "Updating scene..." << std::endl;
  tracer.update();
  std::cout << "Done!" << std::endl;
  tracer.output("raytracer-output2.txt");

  psac::GarbageCollector::run();
}


