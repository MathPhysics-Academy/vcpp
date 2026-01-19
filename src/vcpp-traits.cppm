/*
 *  vcpp:traits - Compile-time parameter binding and extraction
 *
 *  Provides the machinery for VPython-style named parameters:
 *    sphere(pos = vec(0,0,0), radius = 1, color = red)
 *
 *  Uses lam::symbols::symbol_binder and substitution underneath.
 */

module;

import std;

export module vcpp:traits;

import lam.symbols;

export namespace vcpp
{

using namespace lam::symbols;

// ============================================================================
// is_bound - Check if a symbol is bound in a substitution
//
// When you call symbol(substitution) and the symbol isn't bound,
// it returns the symbol itself. We detect this to provide defaults.
// ============================================================================

namespace detail
{

template<typename Symbol, typename Substitution>
struct is_bound_impl
{
  // Call the symbol with the substitution and check return type
  using result_type = decltype(std::declval<Symbol>()(std::declval<Substitution>()));
  static constexpr bool value = !std::is_same_v<std::remove_cvref_t<result_type>, std::remove_cvref_t<Symbol>>;
};

} // namespace detail

template<typename Symbol, typename Substitution>
inline constexpr bool is_bound = detail::is_bound_impl<Symbol, Substitution>::value;

// ============================================================================
// param_spec - Specification for a single parameter
//
// Maps: symbol -> member pointer -> default value
// ============================================================================

template<auto MemberPtr, typename Symbol, auto Default>
struct param_spec
{
  using symbol_type = Symbol;
  static constexpr auto member = MemberPtr;
  static constexpr auto default_value = Default;
};

// ============================================================================
// apply_param - Apply a single parameter to an object
//
// If the symbol is bound in the substitution, extract and assign.
// Otherwise, the object keeps its default value.
// ============================================================================

template<typename Object, typename Substitution, typename ParamSpec>
constexpr void apply_param(Object& obj, const Substitution& params, ParamSpec)
{
  using Symbol = typename ParamSpec::symbol_type;
  constexpr Symbol sym{};

  if constexpr (is_bound<Symbol, Substitution>)
  {
    obj.*(ParamSpec::member) = sym(params);
  }
  // else: object already has default from initialization
}

// ============================================================================
// apply_params - Apply multiple parameters from a tuple of param_specs
// ============================================================================

template<typename Object, typename Substitution, typename... ParamSpecs>
constexpr void apply_params(Object& obj, const Substitution& params, std::tuple<ParamSpecs...>)
{
  (apply_param(obj, params, ParamSpecs{}), ...);
}

// ============================================================================
// object_params - Registry of parameters for each object type
//
// Specialize this for each object type to define its parameters.
// ============================================================================

template<typename ObjectType>
struct object_params
{
  // Default: no object-specific parameters
  static constexpr auto value = std::tuple{};
};

// ============================================================================
// extract_or - Extract a parameter or return default
//
// Useful for one-off extractions without defining a param_spec.
// ============================================================================

template<typename T, typename Symbol, typename Substitution>
constexpr T extract_or(Symbol sym, const Substitution& params, T default_val)
{
  if constexpr (is_bound<Symbol, Substitution>)
  {
    return static_cast<T>(sym(params));
  }
  else
  {
    return default_val;
  }
}

// ============================================================================
// has_param - Check if a parameter was provided (for optional logic)
// ============================================================================

template<typename Symbol, typename... Binders>
constexpr bool has_param(const substitution<Binders...>&)
{
  return is_bound<Symbol, substitution<Binders...>>;
}

} // namespace vcpp
