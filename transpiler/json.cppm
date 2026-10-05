/*
 *  vpy2cpp:json - Reads the JSON syntax tree that vpy_ast.py prints
 *
 *  Numbers keep their source text, so a literal is emitted exactly as written and an int stays
 *  distinct from a float.
 */

module;

import std;

export module vpy2cpp:json;

export namespace vpy2cpp
{

struct json
{
  struct number
  {
    std::string text;
    bool is_integer;
  };
  using array = std::vector<json>;
  using object = std::vector<std::pair<std::string, json>>;

  std::variant<std::nullptr_t, bool, number, std::string, array, object> value;

  bool is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
  const std::string& string() const { return std::get<std::string>(value); }
  const array& items() const { return std::get<array>(value); }
  const number& num() const { return std::get<number>(value); }
  bool boolean() const { return std::get<bool>(value); }

  // The member `key` of an object, or a null json if there is none
  const json& operator[](std::string_view key) const
  {
    static const json null{nullptr};
    if (const auto* obj = std::get_if<object>(&value))
      for (const auto& [k, v] : *obj)
        if (k == key)
          return v;
    return null;
  }
};

class json_reader
{
public:
  static json parse(std::string_view text)
  {
    json_reader r{text};
    json v = r.value();
    r.skip_space();
    if (r.m_at != text.size())
      r.fail("trailing characters");
    return v;
  }

private:
  std::string_view m_text;
  std::size_t m_at{0};

  explicit json_reader(std::string_view text) : m_text(text) {}

  [[noreturn]] void fail(std::string_view what) const
  { throw std::runtime_error(std::format("JSON: {} at offset {}", what, m_at)); }

  void skip_space()
  {
    while (m_at < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_at])))
      ++m_at;
  }

  char peek()
  {
    skip_space();
    if (m_at >= m_text.size())
      fail("unexpected end");
    return m_text[m_at];
  }

  void expect(std::string_view word)
  {
    if (m_text.substr(m_at, word.size()) != word)
      fail(std::format("expected '{}'", word));
    m_at += word.size();
  }

  json value()
  {
    switch (peek())
    {
      case '{':
        return json{object()};
      case '[':
        return json{array()};
      case '"':
        return json{string()};
      case 't':
        expect("true");
        return json{true};
      case 'f':
        expect("false");
        return json{false};
      case 'n':
        expect("null");
        return json{nullptr};
      default:
        return json{number()};
    }
  }

  json::object object()
  {
    json::object out;
    ++m_at;
    if (peek() == '}')
    {
      ++m_at;
      return out;
    }
    while (true)
    {
      if (peek() != '"')
        fail("expected a key");
      std::string key = string();
      if (peek() != ':')
        fail("expected ':'");
      ++m_at;
      out.emplace_back(std::move(key), value());
      const char c = peek();
      ++m_at;
      if (c == '}')
        return out;
      if (c != ',')
        fail("expected ',' or '}'");
    }
  }

  json::array array()
  {
    json::array out;
    ++m_at;
    if (peek() == ']')
    {
      ++m_at;
      return out;
    }
    while (true)
    {
      out.push_back(value());
      const char c = peek();
      ++m_at;
      if (c == ']')
        return out;
      if (c != ',')
        fail("expected ',' or ']'");
    }
  }

  std::string string()
  {
    std::string out;
    ++m_at;
    while (true)
    {
      if (m_at >= m_text.size())
        fail("unterminated string");
      const char c = m_text[m_at++];
      if (c == '"')
        return out;
      if (c != '\\')
      {
        out += c;
        continue;
      }
      if (m_at >= m_text.size())
        fail("unterminated escape");
      const char e = m_text[m_at++];
      switch (e)
      {
        case 'n':
          out += '\n';
          break;
        case 't':
          out += '\t';
          break;
        case 'r':
          out += '\r';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'u':
          append_utf8(out, unicode_escape());
          break;
        default:
          out += e; // " \ /
      }
    }
  }

  std::uint32_t hex4()
  {
    if (m_at + 4 > m_text.size())
      fail("short \\u escape");
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
    {
      const char c = m_text[m_at++];
      v <<= 4;
      if (c >= '0' && c <= '9')
        v |= static_cast<std::uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f')
        v |= static_cast<std::uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        v |= static_cast<std::uint32_t>(c - 'A' + 10);
      else
        fail("bad \\u escape");
    }
    return v;
  }

  std::uint32_t unicode_escape()
  {
    std::uint32_t v = hex4();
    if (v >= 0xD800 && v < 0xDC00 && m_text.substr(m_at, 2) == "\\u") // surrogate pair
    {
      m_at += 2;
      const std::uint32_t low = hex4();
      v = 0x10000 + ((v - 0xD800) << 10) + (low - 0xDC00);
    }
    return v;
  }

  static void append_utf8(std::string& out, std::uint32_t c)
  {
    if (c < 0x80)
      out += static_cast<char>(c);
    else if (c < 0x800)
    {
      out += static_cast<char>(0xC0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
    else if (c < 0x10000)
    {
      out += static_cast<char>(0xE0 | (c >> 12));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
    else
    {
      out += static_cast<char>(0xF0 | (c >> 18));
      out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
  }

  json::number number()
  {
    const std::size_t start = m_at;
    bool integer = true;
    while (m_at < m_text.size())
    {
      const char c = m_text[m_at];
      if (c == '.' || c == 'e' || c == 'E')
        integer = false;
      else if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+'))
        break;
      ++m_at;
    }
    if (m_at == start)
      fail("unexpected character");
    return {std::string(m_text.substr(start, m_at - start)), integer};
  }
};

} // namespace vpy2cpp
