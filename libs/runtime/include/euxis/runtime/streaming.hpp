/// @file
/// @brief Coroutine-based streaming provider API.
///
/// Closes the audit gap "streaming is sync-callback-only" by giving the
/// runtime a `std::generator<ProviderDelta>` surface that delivers
/// incremental text fragments, partial tool-call deltas, and final
/// usage information one chunk at a time. Consumers `co_await`-style
/// iterate a range-for loop and feed each chunk into the agent loop or
/// the UI.
///
/// Toolchain availability
/// ----------------------
/// `<generator>` (P2502 / C++23) is not yet shipped by every standard
/// library that compiles euxis. AppleClang 21's libc++ and Homebrew
/// LLVM 22's libc++ do not include it as of 2026-06; GCC 14's
/// libstdc++ does. The entire IStreamingProvider interface is therefore
/// gated behind `EUXIS_HAS_STD_GENERATOR`, which the header sets when
/// `__cpp_lib_generator` is defined at the right level. Consumers can
/// query the macro to decide whether to compile a streaming code path.
///
/// The non-coroutine pieces of this header (`ProviderDelta`,
/// `DeltaKind`) are unguarded so they remain useful as a wire-format
/// for non-coroutine streaming (e.g. SSE parsers feeding a callback).

#pragma once

#include <cstdint>
#include <string>

#if __has_include(<generator>)
#  include <generator>
#  if defined(__cpp_lib_generator) && (__cpp_lib_generator >= 202207L)
#    define EUXIS_HAS_STD_GENERATOR 1
#  endif
#endif

namespace euxis::runtime {

/// @brief Discriminator for a single streaming delta.
enum class DeltaKind {
    /// Incremental text fragment from the assistant message body.
    TextFragment,
    /// Partial tool-call event — `tool_call_name` / `tool_call_id`
    /// identify the call, and `text` carries an argument-JSON fragment
    /// that the consumer concatenates with prior partials for the same
    /// `tool_call_id` before parsing.
    ToolCallPartial,
    /// Final usage record. Emitted at most once per stream, immediately
    /// before `Done`. Tokens are absolute counts, not deltas.
    Usage,
    /// Stream terminator. The generator returns after yielding this.
    Done,
};

/// @brief One delta from an LLM stream — text, tool-call partial, usage,
///        or terminator.
///
/// Modelled on the OpenAI/Anthropic streaming convention: vendor APIs
/// emit text fragments and tool-call partials interleaved, then a final
/// usage record, then close. The struct is small and trivially copyable
/// so generator yield/return is cheap.
struct ProviderDelta {
    DeltaKind kind{DeltaKind::Done};
    /// TextFragment: assistant text. ToolCallPartial: JSON-arg fragment.
    std::string text;
    /// ToolCallPartial: declared name of the tool being called.
    std::string tool_call_name;
    /// ToolCallPartial: vendor-assigned id used to correlate partials.
    std::string tool_call_id;
    /// Usage: input tokens consumed so far (absolute).
    std::int32_t input_tokens{0};
    /// Usage: output tokens produced so far (absolute).
    std::int32_t output_tokens{0};
};

#if defined(EUXIS_HAS_STD_GENERATOR)

/// @brief Coroutine-based streaming provider interface.
///
/// `execute_stream` returns a `std::generator<ProviderDelta>` the
/// caller iterates lazily. The implementation `co_yield`s text and
/// tool-call partials as they arrive on the wire, ends with a Usage
/// delta, and finally `co_return`s after a `Done` (the Done is
/// optional — the generator's natural end-of-range fires as well).
///
/// This interface is intentionally additive to `IProvider::execute` —
/// callers that don't need streaming keep the existing sync API. No
/// migration is forced.
class IStreamingProvider {
public:
    virtual ~IStreamingProvider() = default;
    IStreamingProvider()                                       = default;
    IStreamingProvider(const IStreamingProvider&)              = delete;
    IStreamingProvider& operator=(const IStreamingProvider&)   = delete;
    IStreamingProvider(IStreamingProvider&&) noexcept          = default;
    IStreamingProvider& operator=(IStreamingProvider&&) noexcept = default;

    /// @brief Stream a model response as a sequence of ProviderDeltas.
    ///
    /// @param model       Model identifier (vendor-specific).
    /// @param prompt      Full prompt text — the caller assembled the
    ///                    conversation transcript before calling.
    /// @param timeout_ms  Per-stream wall-clock budget. Implementations
    ///                    that exceed it should stop yielding and let
    ///                    the generator return naturally so the loop
    ///                    terminates without exceptions.
    ///
    /// Both string parameters are taken **by value**. C++ coroutines
    /// do NOT extend the lifetime of reference parameters across
    /// suspensions (only the *frame* is moved into the promise; the
    /// referent stays at its original location). A `const std::string&`
    /// bound to a temporary at the call site dangles the first time the
    /// generator yields, and ASan catches the dispatch reading dead
    /// stack memory as `[json.exception.type_error.316]` with random
    /// high-bit bytes. See issue #95.
    [[nodiscard]] virtual auto execute_stream(std::string model,
                                              std::string prompt,
                                              int timeout_ms = 30'000)
        -> std::generator<ProviderDelta> = 0;
};

#endif // EUXIS_HAS_STD_GENERATOR

} // namespace euxis::runtime
