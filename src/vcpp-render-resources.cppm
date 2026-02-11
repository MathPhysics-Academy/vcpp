/*
 *  vcpp:render_resources - Resource handle system with generation counters
 *
 *  Provides:
 *  - Typed handles: buffer_handle, texture_handle, sampler_handle
 *  - resource_pool: efficient allocation with use-after-free protection
 *
 *  Generation counters prevent use-after-free bugs by invalidating
 *  handles when their associated resources are released.
 */

module;

import std;

export module vcpp:render_resources;

export namespace vcpp::render
{

// ============================================================================
// Handle Tags - Empty types for type-safe handles
// ============================================================================

struct buffer_tag
{
};
struct texture_tag
{
};
struct sampler_tag
{
};
struct pipeline_tag
{
};
struct bind_group_tag
{
};

// ============================================================================
// handle<Tag> - Typed handle with generation counter
//
// The generation counter protects against use-after-free:
// - When a resource is allocated, its generation is incremented
// - When freed, the slot's generation increases but the index is reused
// - If a stale handle is used, the generation mismatch is detected
// ============================================================================

template<typename Tag>
struct handle
{
  std::uint32_t index{0};
  std::uint32_t generation{0};

  // Default handle is invalid
  constexpr handle() noexcept = default;
  constexpr handle(std::uint32_t idx, std::uint32_t gen) noexcept : index{idx}, generation{gen} {}

  // Check if handle is valid (generation != 0 indicates it was allocated)
  constexpr bool valid() const noexcept { return generation != 0; }

  // Explicit conversion to bool for if-statements
  constexpr explicit operator bool() const noexcept { return valid(); }

  // Comparison operators
  constexpr bool operator==(const handle& other) const noexcept
  {
    return index == other.index && generation == other.generation;
  }
  constexpr bool operator!=(const handle& other) const noexcept { return !(*this == other); }
};

// Type aliases for specific handle types
using buffer_handle = handle<buffer_tag>;
using texture_handle = handle<texture_tag>;
using sampler_handle = handle<sampler_tag>;
using pipeline_handle = handle<pipeline_tag>;
using bind_group_handle = handle<bind_group_tag>;

// ============================================================================
// resource_slot - Internal storage for a single resource
// ============================================================================

template<typename Resource>
struct resource_slot
{
  Resource resource{};
  std::uint32_t generation{0};
  bool in_use{false};
};

// ============================================================================
// resource_pool<Resource, Handle> - Pool allocator for GPU resources
//
// Features:
// - O(1) allocation and deallocation (amortized)
// - Automatic handle invalidation via generation counters
// - Efficient free list for slot reuse
// ============================================================================

template<typename Resource, typename Handle>
class resource_pool
{
public:
  using resource_type = Resource;
  using handle_type = Handle;

  resource_pool() = default;
  ~resource_pool() = default;

  // Non-copyable, movable
  resource_pool(const resource_pool&) = delete;
  resource_pool& operator=(const resource_pool&) = delete;
  resource_pool(resource_pool&&) noexcept = default;
  resource_pool& operator=(resource_pool&&) noexcept = default;

  // Allocate a new slot and return its handle
  Handle allocate(Resource resource)
  {
    std::uint32_t index;

    if (!m_free_list.empty())
    {
      // Reuse a freed slot
      index = m_free_list.back();
      m_free_list.pop_back();
    }
    else
    {
      // Grow the pool
      index = static_cast<std::uint32_t>(m_slots.size());
      m_slots.emplace_back();
    }

    auto& slot = m_slots[index];
    slot.resource = std::move(resource);
    slot.generation++;
    slot.in_use = true;

    return Handle{index, slot.generation};
  }

  // Release a resource by handle
  // Returns true if the handle was valid and the resource was released
  bool release(Handle h)
  {
    if (!is_valid(h))
      return false;

    auto& slot = m_slots[h.index];
    slot.resource = Resource{};
    slot.in_use = false;
    m_free_list.push_back(h.index);
    return true;
  }

  // Check if a handle is valid (points to an in-use slot with matching generation)
  bool is_valid(Handle h) const noexcept
  {
    if (h.index >= m_slots.size())
      return false;

    const auto& slot = m_slots[h.index];
    return slot.in_use && slot.generation == h.generation;
  }

  // Get a pointer to the resource (returns nullptr if invalid)
  Resource* get(Handle h) noexcept
  {
    if (!is_valid(h))
      return nullptr;
    return &m_slots[h.index].resource;
  }

  const Resource* get(Handle h) const noexcept
  {
    if (!is_valid(h))
      return nullptr;
    return &m_slots[h.index].resource;
  }

  // Get reference (throws if invalid)
  Resource& get_ref(Handle h)
  {
    if (!is_valid(h))
      throw std::runtime_error("Invalid resource handle");
    return m_slots[h.index].resource;
  }

  const Resource& get_ref(Handle h) const
  {
    if (!is_valid(h))
      throw std::runtime_error("Invalid resource handle");
    return m_slots[h.index].resource;
  }

  // Clear all resources
  void clear()
  {
    m_slots.clear();
    m_free_list.clear();
  }

  // Get number of active resources
  std::size_t size() const noexcept
  {
    return m_slots.size() - m_free_list.size();
  }

  // Get total capacity (including freed slots)
  std::size_t capacity() const noexcept { return m_slots.size(); }

  // Iterate over all active resources
  template<typename Func>
  void for_each(Func&& func)
  {
    for (auto& slot : m_slots)
    {
      if (slot.in_use)
      {
        func(slot.resource);
      }
    }
  }

  template<typename Func>
  void for_each(Func&& func) const
  {
    for (const auto& slot : m_slots)
    {
      if (slot.in_use)
      {
        func(slot.resource);
      }
    }
  }

private:
  std::vector<resource_slot<Resource>> m_slots;
  std::vector<std::uint32_t> m_free_list;
};

// ============================================================================
// Texture Description
// ============================================================================

enum class texture_format : std::uint8_t
{
  rgba8_unorm,
  bgra8_unorm,
  rgba8_srgb,
  bgra8_srgb,
  rgba16_float,
  rgba32_float,
  depth24_plus,
  depth32_float,
};

enum class texture_usage : std::uint8_t
{
  none = 0,
  sampled = 1 << 0,
  storage = 1 << 1,
  render_target = 1 << 2,
  copy_src = 1 << 3,
  copy_dst = 1 << 4,
};

// Bitwise operators for texture_usage
inline constexpr texture_usage operator|(texture_usage a, texture_usage b) noexcept
{
  return static_cast<texture_usage>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline constexpr texture_usage operator&(texture_usage a, texture_usage b) noexcept
{
  return static_cast<texture_usage>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline constexpr bool has_flag(texture_usage flags, texture_usage flag) noexcept
{
  return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
}

struct texture_desc
{
  std::uint32_t width{1};
  std::uint32_t height{1};
  std::uint32_t depth{1};
  std::uint32_t mip_levels{1};
  texture_format format{texture_format::rgba8_unorm};
  texture_usage usage{texture_usage::sampled};
  bool generate_mipmaps{false};
};

// ============================================================================
// Sampler Description
// ============================================================================

enum class filter_mode : std::uint8_t
{
  nearest,
  linear,
};

enum class address_mode : std::uint8_t
{
  repeat,
  mirror_repeat,
  clamp_to_edge,
  clamp_to_border,
};

struct sampler_desc
{
  filter_mode mag_filter{filter_mode::linear};
  filter_mode min_filter{filter_mode::linear};
  filter_mode mipmap_filter{filter_mode::linear};
  address_mode wrap_u{address_mode::repeat};
  address_mode wrap_v{address_mode::repeat};
  address_mode wrap_w{address_mode::repeat};
  float max_anisotropy{1.0f};
};

// ============================================================================
// Image Data - CPU-side image for texture uploads
// ============================================================================

struct image_data
{
  std::vector<std::uint8_t> pixels;
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::uint32_t channels{4}; // RGBA

  bool valid() const noexcept
  {
    return width > 0 && height > 0 && !pixels.empty() && pixels.size() == width * height * channels;
  }

  // Create a solid color image
  static image_data solid_color(std::uint32_t w, std::uint32_t h, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
  {
    image_data img;
    img.width = w;
    img.height = h;
    img.channels = 4;
    img.pixels.resize(w * h * 4);
    for (std::size_t i = 0; i < w * h; ++i)
    {
      img.pixels[i * 4 + 0] = r;
      img.pixels[i * 4 + 1] = g;
      img.pixels[i * 4 + 2] = b;
      img.pixels[i * 4 + 3] = a;
    }
    return img;
  }

  // Create a checkerboard pattern (useful for debugging)
  static image_data checkerboard(std::uint32_t w, std::uint32_t h, std::uint32_t cell_size = 8)
  {
    image_data img;
    img.width = w;
    img.height = h;
    img.channels = 4;
    img.pixels.resize(w * h * 4);
    for (std::uint32_t y = 0; y < h; ++y)
    {
      for (std::uint32_t x = 0; x < w; ++x)
      {
        bool white = ((x / cell_size) + (y / cell_size)) % 2 == 0;
        std::uint8_t c = white ? 255 : 128;
        std::size_t i = (y * w + x) * 4;
        img.pixels[i + 0] = c;
        img.pixels[i + 1] = c;
        img.pixels[i + 2] = c;
        img.pixels[i + 3] = 255;
      }
    }
    return img;
  }
};

// ============================================================================
// Buffer Description
// ============================================================================

enum class buffer_usage : std::uint8_t
{
  none = 0,
  vertex = 1 << 0,
  index = 1 << 1,
  uniform = 1 << 2,
  storage = 1 << 3,
  copy_src = 1 << 4,
  copy_dst = 1 << 5,
};

inline constexpr buffer_usage operator|(buffer_usage a, buffer_usage b) noexcept
{
  return static_cast<buffer_usage>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline constexpr buffer_usage operator&(buffer_usage a, buffer_usage b) noexcept
{
  return static_cast<buffer_usage>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline constexpr bool has_flag(buffer_usage flags, buffer_usage flag) noexcept
{
  return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
}

struct buffer_desc
{
  std::size_t size{0};
  buffer_usage usage{buffer_usage::vertex};
  bool mapped_at_creation{false};
};

} // namespace vcpp::render
