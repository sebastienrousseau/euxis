/// @file
/// @brief Compile-time JSON-Schema derivation for ToolDeclaration inputs,
///        with a forward-compatible hook for C++26 static reflection.
///
/// Today, registering a tool means hand-writing `input_schema` as a
/// nlohmann::json literal next to a C++ struct that already names every
/// field. The duplication is ergonomically painful and a class of bugs
/// in its own right (rename a field in the struct, forget to update the
/// JSON). This header offers two ways out:
///
///   1. Manual (works today on every C++23 compiler):
///        SchemaBuilder{}.field<std::string>("path")
///                        .field<int>("depth", "How deep to recurse")
///                        .required("path")
///                        .build()
///      Returns a nlohmann::json carrying the same shape the hand-written
///      schema does. Specialise `derive_input_schema<T>` to wrap a struct.
///
///   2. Native reflection (lights up under __cpp_lib_reflection,
///      ~P2996 / C++26 — no shipping compiler implements this yet as
///      of 2026-06; Clang's clang-p2996 branch and EDG ship prototypes):
///      `derive_input_schema<T>()` walks std::meta::nonstatic_data_members_of
///      and emits the same JSON automatically, with field NAMES and
///      TYPES coming from the reflection facility.
///
/// Migration path for SDK consumers
/// --------------------------------
/// // Today
/// struct ScanArgs { std::string path; int depth; };
/// template<> auto derive_input_schema<ScanArgs>() -> nlohmann::json {
///     return SchemaBuilder{}
///         .field<std::string>("path")
///         .field<int>("depth")
///         .required("path")
///         .build();
/// }
///
/// // After P2996 lands (the specialisation above goes away — the
/// // default template below picks it up automatically)
/// struct ScanArgs { std::string path; int depth; };
/// auto schema = derive_input_schema<ScanArgs>();  // just works

#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::runtime {

// ---------------------------------------------------------------------------
// json_type_for<T>() — compile-time C++ → JSON-Schema type-string map
// ---------------------------------------------------------------------------
namespace detail {

template<typename T>
struct json_type_for_impl {
    // Fallback: opaque type → "object" (caller may want to specialise).
    static constexpr std::string_view value = "object";
};

template<> struct json_type_for_impl<bool>            { static constexpr std::string_view value = "boolean"; };
template<> struct json_type_for_impl<std::string>     { static constexpr std::string_view value = "string";  };
template<> struct json_type_for_impl<const char*>     { static constexpr std::string_view value = "string";  };
template<> struct json_type_for_impl<std::string_view>{ static constexpr std::string_view value = "string";  };
template<> struct json_type_for_impl<float>           { static constexpr std::string_view value = "number";  };
template<> struct json_type_for_impl<double>          { static constexpr std::string_view value = "number";  };

template<std::integral T>
    requires (!std::same_as<T, bool>)
struct json_type_for_impl<T> { static constexpr std::string_view value = "integer"; };

template<typename U>
struct json_type_for_impl<std::vector<U>> {
    static constexpr std::string_view value = "array";
};

} // namespace detail

template<typename T>
[[nodiscard]] constexpr auto json_type_for() noexcept -> std::string_view {
    return detail::json_type_for_impl<std::remove_cvref_t<T>>::value;
}

// ---------------------------------------------------------------------------
// SchemaBuilder — fluent compile-time-friendly schema construction
// ---------------------------------------------------------------------------
class SchemaBuilder {
public:
    template<typename T>
    auto field(std::string name, std::string description = {}) -> SchemaBuilder& {
        nlohmann::json entry;
        entry["type"] = std::string{json_type_for<T>()};
        if (!description.empty()) entry["description"] = std::move(description);
        properties_[std::move(name)] = std::move(entry);
        return *this;
    }

    auto required(std::string name) -> SchemaBuilder& {
        required_.push_back(std::move(name));
        return *this;
    }

    [[nodiscard]] auto build() const -> nlohmann::json {
        nlohmann::json out;
        out["type"]       = "object";
        out["properties"] = properties_;
        if (!required_.empty()) out["required"] = required_;
        return out;
    }

private:
    nlohmann::json properties_ = nlohmann::json::object();
    std::vector<std::string> required_;
};

// ---------------------------------------------------------------------------
// derive_input_schema<T> — opt-in point for SDK consumers
// ---------------------------------------------------------------------------
//
// Specialise this template for your tool argument struct. The runtime
// uses it to fill ToolDeclaration_v2::input_schema without duplicating
// the field list between the C++ struct and a JSON literal.
//
// When a compiler ships C++26 reflection, the unspecialised default
// below picks up any aggregate type automatically; consumers can delete
// their hand-written specialisations.
// ---------------------------------------------------------------------------

#if defined(__cpp_lib_reflection)
// Native reflection path — lights up automatically when the compiler
// ships the P2996 facility. Walks every non-static data member of T
// and feeds the SchemaBuilder.
//
// As of 2026-06, no shipping compiler defines __cpp_lib_reflection, so
// this branch is effectively dormant scaffolding. The shape is
// deliberately written out so the migration is a guard removal, not a
// rewrite.
#include <meta>
template<typename T>
[[nodiscard]] auto derive_input_schema() -> nlohmann::json {
    SchemaBuilder b;
    template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T)) {
        using FT = typename [: std::meta::type_of(m) :];
        b.field<FT>(std::string{std::meta::identifier_of(m)});
    }
    return b.build();
}
#else
// Pre-reflection path — primary template intentionally undefined so the
// linker flags any type that ships without a specialisation. Each
// consumer adds:
//
//   template<> auto derive_input_schema<MyArgs>() -> nlohmann::json {
//       return SchemaBuilder{}.field<std::string>("path").build();
//   }
template<typename T>
[[nodiscard]] auto derive_input_schema() -> nlohmann::json;
#endif

} // namespace euxis::runtime
