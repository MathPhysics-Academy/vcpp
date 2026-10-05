/*
 *  vcpp:font - Glyph outlines from a TrueType font
 *
 *  Reads the tables a 'glyf' (quadratic outline) font needs to draw text: head, maxp, hhea, hmtx,
 *  cmap, loca and glyf. Fonts with CFF outlines (most .otf files) are not read.
 */

module;

import std;

export module vcpp:font;

import :vec;

export namespace vcpp
{

class truetype_font
{
public:
  // A glyph's closed outlines and advance width, in em units (the font's units per em = 1)
  struct glyph
  {
    std::vector<std::vector<vec2>> contours;
    double advance{0};
  };

  // nullopt if the data is not a TrueType font this reader handles
  static std::optional<truetype_font> parse(std::vector<std::byte> data)
  {
    truetype_font f;
    f.m_data = std::move(data);
    try
    {
      if (!f.read_tables())
        return std::nullopt;
    }
    catch (const std::out_of_range&)
    {
      return std::nullopt;
    }
    return f;
  }

  static std::optional<truetype_font> load(const std::string& path)
  {
    std::ifstream in(path, std::ios::binary);
    if (!in)
      return std::nullopt;
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> data(bytes.size());
    std::ranges::transform(bytes, data.begin(), [](char b) { return static_cast<std::byte>(b); });
    return parse(std::move(data));
  }

  // The outline of character c; each quadratic curve becomes curve_steps line segments
  glyph outline(char32_t c, int curve_steps = 6) const
  {
    glyph g;
    try
    {
      const std::uint32_t id = glyph_index(c);
      g.advance = advance_width(id) / m_units_per_em;
      append_outline(g.contours, id, {1, 0, 0, 1, 0, 0}, curve_steps, 0);
    }
    catch (const std::out_of_range&)
    {
      g.contours.clear(); // a malformed glyph draws as nothing
    }
    for (auto& contour : g.contours)
      for (auto& p : contour)
        p = vec2{p.x() / m_units_per_em, p.y() / m_units_per_em};
    return g;
  }

private:
  std::vector<std::byte> m_data;
  double m_units_per_em{1000};
  std::uint32_t m_num_glyphs{0};
  std::uint32_t m_num_hmetrics{0};
  bool m_long_loca{false};
  std::size_t m_hmtx{0};
  std::size_t m_loca{0};
  std::size_t m_glyf{0};
  std::size_t m_cmap_subtable{0};
  std::uint16_t m_cmap_format{0};

  // A 2x3 affine transform: x' = a x + c y + e, y' = b x + d y + f
  struct affine
  {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
  };

  std::uint8_t u8(std::size_t at) const { return std::to_integer<std::uint8_t>(m_data.at(at)); }
  std::uint16_t u16(std::size_t at) const { return static_cast<std::uint16_t>(u8(at) << 8 | u8(at + 1)); }
  std::int16_t i16(std::size_t at) const { return static_cast<std::int16_t>(u16(at)); }
  std::uint32_t u32(std::size_t at) const { return std::uint32_t{u16(at)} << 16 | u16(at + 2); }

  std::optional<std::size_t> find_table(std::string_view tag) const
  {
    const std::size_t num_tables = u16(4);
    for (std::size_t i = 0; i < num_tables; ++i)
    {
      const std::size_t record = 12 + 16 * i;
      bool match = true;
      for (std::size_t k = 0; k < 4; ++k)
        match = match && u8(record + k) == static_cast<std::uint8_t>(tag[k]);
      if (match)
        return u32(record + 8);
    }
    return std::nullopt;
  }

  bool read_tables()
  {
    const std::uint32_t version = u32(0);
    if (version != 0x00010000 && version != 0x74727565) // 1.0 or 'true'; 'OTTO' is CFF
      return false;
    const auto head = find_table("head");
    const auto maxp = find_table("maxp");
    const auto hhea = find_table("hhea");
    const auto hmtx = find_table("hmtx");
    const auto loca = find_table("loca");
    const auto glyf = find_table("glyf");
    const auto cmap = find_table("cmap");
    if (!head || !maxp || !hhea || !hmtx || !loca || !glyf || !cmap)
      return false;

    m_units_per_em = u16(*head + 18);
    m_long_loca = i16(*head + 50) != 0;
    m_num_glyphs = u16(*maxp + 4);
    m_num_hmetrics = u16(*hhea + 34);
    m_hmtx = *hmtx;
    m_loca = *loca;
    m_glyf = *glyf;
    if (m_units_per_em == 0 || m_num_hmetrics == 0)
      return false;

    // Prefer a full-Unicode (format 12) map, else a BMP (format 4) one
    const std::size_t num_maps = u16(*cmap + 2);
    for (std::size_t i = 0; i < num_maps; ++i)
    {
      const std::size_t record = *cmap + 4 + 8 * i;
      const std::uint16_t platform = u16(record);
      const std::uint16_t encoding = u16(record + 2);
      const std::size_t sub = *cmap + u32(record + 4);
      const std::uint16_t format = u16(sub);
      const bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
      if (!unicode || (format != 4 && format != 12))
        continue;
      if (m_cmap_format != 12)
      {
        m_cmap_subtable = sub;
        m_cmap_format = format;
      }
    }
    return m_cmap_format != 0;
  }

  std::uint32_t glyph_index(char32_t c) const
  {
    const std::size_t sub = m_cmap_subtable;
    if (m_cmap_format == 12)
    {
      const std::uint32_t groups = u32(sub + 12);
      for (std::uint32_t i = 0; i < groups; ++i)
      {
        const std::size_t group = sub + 16 + 12 * std::size_t{i};
        const std::uint32_t first = u32(group);
        const std::uint32_t last = u32(group + 4);
        if (c >= first && c <= last)
          return u32(group + 8) + (c - first);
      }
      return 0;
    }
    if (c > 0xFFFF)
      return 0;
    const std::size_t segments = u16(sub + 6) / 2;
    const std::size_t ends = sub + 14;
    const std::size_t starts = ends + 2 * segments + 2;
    const std::size_t deltas = starts + 2 * segments;
    const std::size_t range_offsets = deltas + 2 * segments;
    for (std::size_t i = 0; i < segments; ++i)
    {
      if (c > u16(ends + 2 * i))
        continue;
      const std::uint16_t start = u16(starts + 2 * i);
      if (c < start)
        return 0;
      const std::uint16_t delta = u16(deltas + 2 * i);
      const std::uint16_t range_offset = u16(range_offsets + 2 * i);
      if (range_offset == 0)
        return static_cast<std::uint16_t>(c + delta);
      const std::size_t at = range_offsets + 2 * i + range_offset + 2 * (c - start);
      const std::uint16_t id = u16(at);
      return id == 0 ? 0 : static_cast<std::uint16_t>(id + delta);
    }
    return 0;
  }

  double advance_width(std::uint32_t id) const
  {
    const std::uint32_t metric = std::min(id, m_num_hmetrics - 1);
    return u16(m_hmtx + 4 * std::size_t{metric});
  }

  std::pair<std::size_t, std::size_t> glyph_range(std::uint32_t id) const
  {
    if (id >= m_num_glyphs)
      return {0, 0};
    if (m_long_loca)
      return {u32(m_loca + 4 * std::size_t{id}), u32(m_loca + 4 * std::size_t{id} + 4)};
    return {std::size_t{u16(m_loca + 2 * std::size_t{id})} * 2, std::size_t{u16(m_loca + 2 * std::size_t{id} + 2)} * 2};
  }

  void append_outline(std::vector<std::vector<vec2>>& out, std::uint32_t id, const affine& t, int steps,
                      int depth) const
  {
    const auto [begin, end] = glyph_range(id);
    if (end <= begin || depth > 8)
      return;
    const std::size_t at = m_glyf + begin;
    const std::int16_t num_contours = i16(at);
    if (num_contours >= 0)
      append_simple(out, at, num_contours, t, steps);
    else
      append_composite(out, at + 10, t, steps, depth);
  }

  void append_simple(std::vector<std::vector<vec2>>& out, std::size_t at, std::size_t num_contours, const affine& t,
                     int steps) const
  {
    std::vector<std::size_t> last_point(num_contours);
    for (std::size_t i = 0; i < num_contours; ++i)
      last_point[i] = u16(at + 10 + 2 * i);
    if (num_contours == 0)
      return;
    const std::size_t num_points = last_point.back() + 1;
    std::size_t p = at + 10 + 2 * num_contours;
    p += 2 + u16(p); // skip the instructions

    std::vector<std::uint8_t> flags;
    flags.reserve(num_points);
    while (flags.size() < num_points)
    {
      const std::uint8_t flag = u8(p++);
      flags.push_back(flag);
      if (flag & 0x08) // repeated
        for (std::uint8_t r = u8(p++); r > 0 && flags.size() < num_points; --r)
          flags.push_back(flag);
    }

    auto read_coords = [&](std::uint8_t short_bit, std::uint8_t same_bit) {
      std::vector<double> coords(num_points);
      double value = 0;
      for (std::size_t i = 0; i < num_points; ++i)
      {
        if (flags[i] & short_bit)
        {
          const std::uint8_t d = u8(p++);
          value += (flags[i] & same_bit) ? d : -static_cast<double>(d);
        }
        else if (!(flags[i] & same_bit))
        {
          value += i16(p);
          p += 2;
        }
        coords[i] = value;
      }
      return coords;
    };
    const auto xs = read_coords(0x02, 0x10);
    const auto ys = read_coords(0x04, 0x20);

    std::size_t first = 0;
    for (std::size_t c = 0; c < num_contours; ++c)
    {
      std::vector<vec2> points;
      std::vector<bool> on_curve;
      for (std::size_t i = first; i <= last_point[c] && i < num_points; ++i)
      {
        points.push_back(vec2{t.a * xs[i] + t.c * ys[i] + t.e, t.b * xs[i] + t.d * ys[i] + t.f});
        on_curve.push_back(flags[i] & 0x01);
      }
      first = last_point[c] + 1;
      if (points.size() >= 3)
        out.push_back(flatten(points, on_curve, steps));
    }
  }

  void append_composite(std::vector<std::vector<vec2>>& out, std::size_t p, const affine& t, int steps, int depth) const
  {
    while (true)
    {
      const std::uint16_t flags = u16(p);
      const std::uint16_t id = u16(p + 2);
      p += 4;
      double dx = 0;
      double dy = 0;
      if (flags & 0x0001) // 16-bit arguments
      {
        dx = i16(p);
        dy = i16(p + 2);
        p += 4;
      }
      else
      {
        dx = static_cast<std::int8_t>(u8(p));
        dy = static_cast<std::int8_t>(u8(p + 1));
        p += 2;
      }
      if (!(flags & 0x0002)) // arguments are point numbers to match, which this reader doesn't do
      {
        dx = 0;
        dy = 0;
      }
      auto f2dot14 = [&](std::size_t at) { return i16(at) / 16384.0; };
      affine part{1, 0, 0, 1, dx, dy};
      if (flags & 0x0008) // one scale
      {
        part.a = part.d = f2dot14(p);
        p += 2;
      }
      else if (flags & 0x0040) // x and y scales
      {
        part.a = f2dot14(p);
        part.d = f2dot14(p + 2);
        p += 4;
      }
      else if (flags & 0x0080) // 2x2 matrix
      {
        part.a = f2dot14(p);
        part.b = f2dot14(p + 2);
        part.c = f2dot14(p + 4);
        part.d = f2dot14(p + 6);
        p += 8;
      }
      const affine combined{t.a * part.a + t.c * part.b,       t.b * part.a + t.d * part.b,
                            t.a * part.c + t.c * part.d,       t.b * part.c + t.d * part.d,
                            t.a * part.e + t.c * part.f + t.e, t.b * part.e + t.d * part.f + t.f};
      append_outline(out, id, combined, steps, depth + 1);
      if (!(flags & 0x0020)) // no more components
        break;
    }
  }

  // A contour of on- and off-curve points as a closed polyline; between two off-curve points there is an
  // implied on-curve point at their midpoint
  static std::vector<vec2> flatten(const std::vector<vec2>& points, const std::vector<bool>& on_curve, int steps)
  {
    const std::size_t n = points.size();
    auto mid = [](const vec2& a, const vec2& b) { return vec2{(a.x() + b.x()) / 2, (a.y() + b.y()) / 2}; };

    std::size_t start = 0;
    while (start < n && !on_curve[start])
      ++start;
    vec2 first = start < n ? points[start] : mid(points[0], points[1]);
    if (start == n)
      start = 0;

    std::vector<vec2> out{first};
    vec2 current = first;
    std::optional<vec2> control;
    for (std::size_t k = 1; k <= n; ++k)
    {
      const std::size_t i = (start + k) % n;
      const vec2& p = points[i];
      if (on_curve[i])
      {
        if (control)
          append_curve(out, current, *control, p, steps);
        else
          out.push_back(p);
        current = p;
        control.reset();
      }
      else
      {
        if (control)
        {
          const vec2 implied = mid(*control, p);
          append_curve(out, current, *control, implied, steps);
          current = implied;
        }
        control = p;
      }
    }
    if (control)
      append_curve(out, current, *control, first, steps);
    if (out.size() > 1 && out.back() == out.front())
      out.pop_back();
    return out;
  }

  static void append_curve(std::vector<vec2>& out, const vec2& from, const vec2& control, const vec2& to, int steps)
  {
    for (int s = 1; s <= steps; ++s)
    {
      const double u = static_cast<double>(s) / steps;
      const double v = 1 - u;
      out.push_back(vec2{v * v * from.x() + 2 * v * u * control.x() + u * u * to.x(),
                         v * v * from.y() + 2 * v * u * control.y() + u * u * to.y()});
    }
  }
};

} // namespace vcpp
