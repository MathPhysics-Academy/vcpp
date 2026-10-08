/*
 *  test_named_params.cpp - Named parameters reach the members they name
 *
 *  Each case uses a parameter that make<T>() used to ignore silently.
 *  Passing a parameter an object doesn't take is now a compile error, so
 *  that half is checked by building, not here.
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

bool near(double a, double b) { return std::abs(a - b) < 1e-12; }
bool near(const vec3& a, const vec3& b) { return near(a.x(), b.x()) && near(a.y(), b.y()) && near(a.z(), b.z()); }

// box: size = (length, height, width); length also rescales axis, as in GlowScript
bool test_box_size()
{
  auto b = build::box(size = vec3{2, 3, 4});
  return near(b.m_length, 2) && near(b.m_height, 3) && near(b.m_width, 4) && near(b.m_axis, vec3{2, 0, 0});
}

// GlowScript applies axis before size, so size.x wins and keeps axis's direction
bool test_size_overrides_axis_length()
{
  auto p = build::pyramid(axis = vec3{0, 3, 0}, size = vec3{2, 2, 2});
  return near(p.m_length, 2) && near(p.m_axis, vec3{0, 2, 0});
}

// With no length or size, axis sets the length
bool test_axis_sets_length()
{
  auto b = build::box(axis = vec3{0, 0, 5});
  return near(b.m_length, 5) && near(b.m_axis, vec3{0, 0, 5});
}

// cylinder: size = (length, 2r, 2r); the renderer draws length from mag(axis)
bool test_cylinder_size()
{
  auto c = build::cylinder(axis = vec3{0, 1, 0}, size = vec3{6, 0.4, 0.4});
  return near(c.m_length, 6) && near(c.m_radius, 0.2) && near(c.m_axis, vec3{0, 6, 0});
}

// length= used to set m_length only, which the cylinder renderer never reads
bool test_cylinder_length_moves_axis()
{
  auto c = build::cylinder(length = 3.0);
  return near(c.m_axis, vec3{3, 0, 0});
}

// A size the members can hold exactly is applied...
bool test_sphere_uniform_size()
{
  auto s = build::sphere(size = 0.4 * vec3{1, 1, 1});
  return near(s.m_radius, 0.2);
}

// ...and one they can't throws, rather than being cut down to fit
bool test_lossy_size_throws()
{
  bool sphere_threw = false;
  bool cylinder_threw = false;
  try
  {
    build::sphere(size = vec3{3, 2, 1});
  }
  catch (const std::invalid_argument&)
  {
    sphere_threw = true;
  }
  try
  {
    build::cylinder(size = vec3{1, 2, 1});
  }
  catch (const std::invalid_argument&)
  {
    cylinder_threw = true;
  }
  return sphere_threw && cylinder_threw;
}

bool test_ellipsoid_size()
{
  auto e = build::ellipsoid(size = vec3{0.3, 1.5, 1.5});
  return near(e.m_length, 0.3) && near(e.m_height, 1.5) && near(e.m_width, 1.5);
}

bool test_label_params()
{
  auto l = build::label(text = std::string{"hello"}, prop::box = true);
  return l.m_text == "hello" && l.m_box;
}

bool test_text3d_params()
{
  auto t = build::text3d(text = std::string{"VPython"}, align = std::string{"center"}, font = std::string{"serif"});
  return t.m_text == "VPython" && t.m_align == "center" && t.m_font == "serif";
}

// As for GlowScript's compound, pos is the centre of the extrusion's bounding box unless it is given
bool test_extrusion_params()
{
  const std::vector<vec3> line{{0, 0, 0}, {0, 0, -2}};
  const std::vector<vec2> square{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
  auto e = build::extrusion(path = line, shape = square, twist = 0.5, scale = 0.25);
  auto placed = build::extrusion(path = line, shape = square, pos = vec3{5, 0, 0});
  return e.m_path.size() == 2 && e.m_shape.shapes[0][0].size() == 5 && near(*e.m_twist.single, 0.5) &&
         near(*e.m_scale.single, 0.25) && near(e.m_pos, vec3{0, 0, -1}) && near(placed.m_pos, vec3{5, 0, 0});
}

bool test_graph_params()
{
  auto g = graph(title = std::string{"Energy"}, xtitle = std::string{"t (s)"}, width = 500);
  auto c = gcurve(graph_ref = g, prop::label = std::string{"KE"}, color = vec3{1, 0, 0});
  auto empty = graph();
  return g.m_title == "Energy" && g.m_xtitle == "t (s)" && g.m_width == 500 && c.m_graph_id == g.m_id &&
         c.m_label == "KE" && near(c.m_color, vec3{1, 0, 0}) && empty.m_id != g.m_id;
}

// As in GlowScript, a ring given only a radius is 0.1 x radius thick; an explicit thickness wins
bool test_ring_thickness()
{
  auto r = build::ring(radius = 2.0);
  auto t = build::ring(radius = 2.0, thickness = 0.05);
  return near(r.m_thickness, 0.2) && near(t.m_thickness, 0.05) && near(t.m_radius, 2.0) &&
         near(build::helix().m_thickness, 0);
}

// Untouched objects keep their defaults, including length == mag(axis)
bool test_defaults_unchanged()
{
  auto b = build::box();
  auto c = build::cylinder(radius = 0.5);
  return near(b.m_length, 1) && near(b.m_axis, vec3{1, 0, 0}) && near(c.m_axis, vec3{1, 0, 0}) && near(c.m_radius, 0.5);
}

} // namespace

int main()
{
  int passed = 0;
  int failed = 0;

  auto run_test = [&](const char* name, bool (*test)()) {
    if (test())
    {
      std::println("  ✓ {}", name);
      ++passed;
    }
    else
    {
      std::println("  ✗ {} FAILED", name);
      ++failed;
    }
  };

  std::println("vcpp named-parameter tests");
  std::println("==========================");

  run_test("box size", test_box_size);
  run_test("size overrides axis length", test_size_overrides_axis_length);
  run_test("axis sets length", test_axis_sets_length);
  run_test("cylinder size", test_cylinder_size);
  run_test("cylinder length moves axis", test_cylinder_length_moves_axis);
  run_test("sphere uniform size", test_sphere_uniform_size);
  run_test("lossy size throws", test_lossy_size_throws);
  run_test("ellipsoid size", test_ellipsoid_size);
  run_test("label params", test_label_params);
  run_test("text3d params", test_text3d_params);
  run_test("extrusion params", test_extrusion_params);
  run_test("graph params", test_graph_params);
  run_test("ring thickness", test_ring_thickness);
  run_test("defaults unchanged", test_defaults_unchanged);

  std::println("==========================");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
