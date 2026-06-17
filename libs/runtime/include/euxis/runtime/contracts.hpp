/// @file
/// @brief Forward-compatible C++26 contracts shim.
///
/// The C++26 paper (P2900) standardises natural-syntax preconditions,
/// postconditions, and `contract_assert`. As of 2026-06, no shipping
/// compiler implements it: Apple Clang 21 does not, upstream Clang's
/// implementation lives behind `-fcontracts` on an experimental branch,
/// GCC has nothing landed. This header lets euxis adopt the *semantics*
/// today and pick up the native syntax the day a compiler ships it,
/// without churning every call site.
///
/// The shim resolves in priority order:
///   1. __cpp_contracts >= 202403L  → native pre / post / contract_assert
///   2. EUXIS_CONTRACTS_RUNTIME     → runtime check, abort on violation
///   3. EUXIS_CONTRACTS_ASSUME      → [[assume(...)]] optimiser hint (C++23)
///   4. (else)                       → no-op (matches P2900 "ignore" semantic)
///
/// CMake-side flag: -DEUXIS_ENABLE_CXX26_CONTRACTS=ON enables path #2,
/// turning the annotations into death-test-able runtime checks.
///
/// SAFETY NOTE on [[assume]]
/// -------------------------
/// Path 3 is explicit opt-in (not the default) because C++26 `pre`/`post`
/// semantics permit the predicate to be false at runtime — the violation
/// handler runs, but the function still executes. `[[assume]]` does NOT
/// match those semantics: if its predicate is false, the program has
/// undefined behaviour and the optimiser is licensed to miscompile the
/// surrounding code. Mixing the two without thought is a footgun. The
/// default (path 4) is the safe `((void)0)` resolution that matches the
/// P2900 `ignore` semantic exactly.
///
/// Pilot scope (libs/runtime as of this commit):
///   * WindowedContextEngine::plan postcondition (partition exhaustiveness)
///   * IterationBudget::refund precondition (cannot refund a full budget)

#pragma once

#if defined(__cpp_contracts) && (__cpp_contracts >= 202403L)
    // Path 1: native C++26 contracts. The preprocessor cannot inject
    // `pre`/`post` clauses into a function declaration via macros without
    // help — the clauses appear *after* the parameter list and *before*
    // the body. Consumers should write them directly in that mode. For
    // call-site invariants, contract_assert works identically.
    #define EUXIS_CONTRACT_ASSERT(expr) contract_assert(expr)
    // pre / post: documented as language constructs; the macros expand
    // away because the source uses them directly under path 1.
    #define EUXIS_PRE(expr)
    #define EUXIS_POST(expr)
#elif defined(EUXIS_CONTRACTS_RUNTIME)
    // Path 2: runtime check, abort on violation. Equivalent to assert(),
    // unconditional (does not honour NDEBUG) so the pilot remains useful
    // in optimised builds. Wraps in a do/while(0) so it can be used as a
    // statement.
    #include <cstdio>
    #include <cstdlib>
    #define EUXIS_CONTRACT_ASSERT(expr)                                 \
        do {                                                            \
            if (!(expr)) {                                              \
                std::fprintf(stderr,                                    \
                    "euxis: contract violation [%s:%d] %s\n",           \
                    __FILE__, __LINE__, #expr);                         \
                std::abort();                                           \
            }                                                           \
        } while (0)
    #define EUXIS_PRE(expr)  EUXIS_CONTRACT_ASSERT(expr)
    #define EUXIS_POST(expr) EUXIS_CONTRACT_ASSERT(expr)
#elif defined(EUXIS_CONTRACTS_ASSUME) && \
      defined(__has_cpp_attribute) && __has_cpp_attribute(assume) >= 202207L
    // Path 3 (EXPLICIT OPT-IN): C++23 [[assume]] optimiser hint, no
    // runtime cost. Wrong assumptions become undefined behaviour, so
    // this path is only appropriate when the predicate is genuinely an
    // invariant of surrounding code, not a defensive check on caller
    // input. Enable per translation unit by defining
    // EUXIS_CONTRACTS_ASSUME; we do NOT default it on because doing so
    // would silently turn legitimate precondition failures into UB.
    #define EUXIS_CONTRACT_ASSERT(expr) [[assume(expr)]]
    #define EUXIS_PRE(expr)  [[assume(expr)]]
    #define EUXIS_POST(expr) [[assume(expr)]]
#else
    // Path 4: default. Equivalent to P2900's `ignore` semantic. Nothing
    // is emitted; predicate text is preserved at the call site purely
    // for documentation. This is the safe default — annotated functions
    // run their full bodies on a precondition violation and the caller
    // can observe a normal (false / empty / fallback) return value.
    #define EUXIS_CONTRACT_ASSERT(expr) ((void)0)
    #define EUXIS_PRE(expr)             ((void)0)
    #define EUXIS_POST(expr)            ((void)0)
#endif
