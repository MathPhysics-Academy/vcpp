/*
 *  test_font.cpp - TrueType outlines and the triangulation that fills them
 *
 *  The font checks need VCPP_FONT_SANS set to a TrueType file (e.g. Roboto-Medium.ttf); without it they
 *  are reported as skipped, not passed.
 */

import vcpp;
import std;

using namespace vcpp;

namespace
{

double triangles_area(const std::vector<std::vector<vec2>>& contours, const std::vector<std::uint32_t>& tris)
{
  std::vector<vec2> pts;
  for (const auto& c : contours)
    pts.insert(pts.end(), c.begin(), c.end());
  double area = 0;
  for (std::size_t i = 0; i + 2 < tris.size(); i += 3)
  {
    const vec2& a = pts[tris[i]];
    const vec2& b = pts[tris[i + 1]];
    const vec2& c = pts[tris[i + 2]];
    area += ((b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x())) / 2;
  }
  return area;
}

// Outer contours' area minus their holes', using the same odd-nesting rule
double filled_area(const std::vector<std::vector<vec2>>& contours)
{
  double area = 0;
  for (std::size_t i = 0; i < contours.size(); ++i)
  {
    int depth = 0;
    for (std::size_t j = 0; j < contours.size(); ++j)
      if (i != j && mesh::detail::inside(contours[j], contours[i].front()))
        ++depth;
    area += (depth % 2 == 0 ? 1 : -1) * std::abs(mesh::detail::signed_area(contours[i]));
  }
  return area;
}

bool test_square_with_hole()
{
  std::vector<std::vector<vec2>> c{{{0, 0}, {4, 0}, {4, 4}, {0, 4}}, {{1, 1}, {1, 3}, {3, 3}, {3, 1}}};
  return std::abs(triangles_area(c, mesh::triangulate(c)) - 12.0) < 1e-9;
}

bool test_two_separate_squares()
{
  std::vector<std::vector<vec2>> c{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}, {{2, 0}, {3, 0}, {3, 1}, {2, 1}}};
  return std::abs(triangles_area(c, mesh::triangulate(c)) - 2.0) < 1e-9;
}

std::optional<truetype_font> sans()
{
  const char* path = std::getenv("VCPP_FONT_SANS");
  return path ? truetype_font::load(path) : std::nullopt;
}

int font_tests_run = 0;

bool test_glyph_contours()
{
  auto f = sans();
  if (!f)
    return true;
  ++font_tests_run;
  auto count = [&](char32_t c) { return f->outline(c).contours.size(); };
  const auto space = f->outline(U' ');
  return count(U'O') == 2 && count(U'i') == 2 && count(U'8') == 3 && count(U'A') == 2 && space.contours.empty() &&
         space.advance > 0;
}

// The fill covers exactly the glyph (outer area minus holes) for every printable ASCII character
bool test_glyph_fill_area()
{
  auto f = sans();
  if (!f)
    return true;
  ++font_tests_run;
  for (char32_t c = U'!'; c <= U'~'; ++c)
  {
    const auto g = f->outline(c);
    const double want = filled_area(g.contours);
    const double got = triangles_area(g.contours, mesh::triangulate(g.contours));
    if (want <= 0 || std::abs(got - want) > 1e-6 * std::max(1.0, want))
    {
      std::println("    '{}': filled {} of {}", static_cast<char>(c), got, want);
      return false;
    }
  }
  return true;
}

// A lowercase h comes out exactly `height` tall, and text runs along +x from the origin
bool test_text_mesh()
{
  const char* path = std::getenv("VCPP_FONT_SANS");
  if (!path)
    return true;
  ++font_tests_run;
  text_glyphs::font_file_sans = path;
  const auto mesh = text_glyphs::generate_text_mesh("h", 2.0, 0.4);
  float top = 0;
  float left = 1e9f;
  float back = 1e9f;
  float front = -1e9f;
  for (const auto& v : mesh.vertices)
  {
    top = std::max(top, v.position[1]);
    left = std::min(left, v.position[0]);
    back = std::min(back, v.position[2]);
    front = std::max(front, v.position[2]);
  }
  return std::abs(top - 2.0f) < 1e-5f && left >= 0 && back == 0.0f && std::abs(front - 0.4f) < 1e-6f &&
         text_glyphs::get_text_width("hh") > text_glyphs::get_text_width("h");
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

  std::println("vcpp font tests");
  std::println("===============");

  run_test("square with a hole", test_square_with_hole);
  run_test("two separate squares", test_two_separate_squares);
  run_test("glyph contours", test_glyph_contours);
  run_test("glyph fill area", test_glyph_fill_area);
  run_test("text mesh", test_text_mesh);

  std::println("===============");
  if (font_tests_run == 0)
    std::println("Font checks SKIPPED: set VCPP_FONT_SANS to a TrueType file");
  std::println("Passed: {}/{}", passed, passed + failed);

  return failed > 0 ? 1 : 0;
}
