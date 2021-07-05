#include "gtest/gtest.h"
#include "common.hpp"

#include <psac/psac.hpp>
#include <psac/examples/raytracer.hpp>

constexpr int SCENE_SIZE = 200;

INSTANTIATE_MT_TESTS(TestRaytracer);

// Test that the raytracer doesn't crash on the initial run
TEST_P(TestRaytracer, TestRaytracerInitialRun) {
  auto [scene, ball_ptr] = Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  RayTracer tracer(SCENE_SIZE, SCENE_SIZE, 3, 0.3, scene, camera);
  tracer.go();

  psac::GarbageCollector::run();
}

// Test that the raytracer doesn't crash when updating
TEST_P(TestRaytracer, TestRaytracerUpdate) {
  auto [scene, ball_ptr] = Scene::default_scene();
  Camera camera(Vector(0, -10, 3), Vector(0, 1, -0.2), Vector(0, 0.2, 1));
  RayTracer tracer(SCENE_SIZE, SCENE_SIZE, 3, 0.3, scene, camera);
  tracer.go();
  ball_ptr->change_colour(Colour(0.1, 0.35, 0.90));

  // Propagate changes
  tracer.update();

  psac::GarbageCollector::run();
}

