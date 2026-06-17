/// @file
/// @brief Google Benchmark suite for the scan + evidence pipeline.
///
/// Pre-rewrite, libs/bench measured crypto primitives in isolation
/// (AES-GCM throughput, BLAKE2b key derivation, agent-card msgpack).
/// Those numbers were honest but disconnected from the user-visible
/// "scan a repo" story. This suite measures the actual evidence
/// pipeline the v0.1.0-beta CI workflows exercise:
///
///   1. SCA parser throughput        (Cargo.lock, package-lock.json)
///   2. SBOM emission (CycloneDX 1.6, SPDX 3.0.1)
///   3. OpenVEX statement emission
///   4. DSSE sign + verify round-trip
///   5. Slopsquatting guard lookup throughput
///   6. ScanCache get/put round-trip
///
/// The benchmarks are deterministic and self-contained — no on-disk
/// fixtures beyond a tempfile-scoped SQLite cache. Output is the
/// standard `benchmark::` JSON shape, which bencher.dev and the
/// github-action-benchmark trend tracker both consume.
///
/// Built only when `-DEUXIS_BUILD_GBENCH=ON` is passed.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <benchmark/benchmark.h>

#include "euxis/attest/dsse.hpp"
#include "euxis/attest/signing.hpp"
#include "euxis/attest/slsa.hpp"
#include "euxis/attest/statement.hpp"
#include "euxis/cache/hash.hpp"
#include "euxis/cache/key.hpp"
#include "euxis/cache/store.hpp"
#include "euxis/crypto/ed25519.hpp"
#include "euxis/sbom/cyclonedx.hpp"
#include "euxis/sbom/openvex.hpp"
#include "euxis/sbom/spdx.hpp"
#include "euxis/sbom/types.hpp"
#include "euxis/sca/cargo_lock.hpp"
#include "euxis/sca/npm_lock.hpp"
#include "euxis/slopsquatting/guard.hpp"

namespace {

// ---- fixtures -------------------------------------------------------------

/// Build a SbomDocument with `n` components and a simple chain of
/// dependencies. Used by the CycloneDX / SPDX / OpenVEX emitters.
auto make_synthetic_sbom(std::size_t n) -> euxis::sbom::SbomDocument {
    euxis::sbom::SbomDocument doc;
    doc.serial_number = "urn:uuid:00000000-0000-4000-8000-000000000000";
    doc.tools.push_back({"euxis.co", "euxis-cli", "0.1.0-bench"});

    euxis::sbom::Component root;
    root.bom_ref = "euxis:bench-root";
    root.name    = "bench-root";
    root.version = "0.1.0";
    root.type    = euxis::sbom::ComponentType::Application;
    doc.root = root;

    doc.components.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        euxis::sbom::Component c;
        c.name    = "pkg-" + std::to_string(i);
        c.version = "1.0." + std::to_string(i);
        c.purl    = "pkg:cargo/" + c.name + "@" + c.version;
        c.bom_ref = c.purl;
        c.licenses.push_back({"MIT", std::nullopt});
        c.hashes.push_back({euxis::sbom::HashAlgorithm::Sha256,
                            std::string(64, 'a')});
        doc.components.push_back(std::move(c));
    }
    return doc;
}

/// Build a synthetic Cargo.lock fixture with `n` `[[package]]` blocks.
auto make_cargo_lock_fixture(std::size_t n) -> std::string {
    std::ostringstream ss;
    ss << "# auto-generated\nversion = 3\n\n";
    for (std::size_t i = 0; i < n; ++i) {
        ss << "[[package]]\n"
              "name = \"pkg-" << i << "\"\n"
              "version = \"1.0." << i << "\"\n"
              "source = \"registry+https://github.com/rust-lang/crates.io-index\"\n"
              "checksum = \"" << std::string(64, 'a') << "\"\n\n";
    }
    return ss.str();
}

/// Build a synthetic package-lock.json v3 with `n` entries.
auto make_npm_lock_fixture(std::size_t n) -> std::string {
    std::ostringstream ss;
    ss << R"({"name":"bench","version":"1.0.0","lockfileVersion":3,"packages":{)";
    ss << R"("":{"name":"bench","version":"1.0.0"})";
    for (std::size_t i = 0; i < n; ++i) {
        ss << ",\"node_modules/pkg-" << i << "\":{\"version\":\"1.0."
           << i << "\"}";
    }
    ss << "}}";
    return ss.str();
}

auto make_test_statement() -> euxis::attest::Statement {
    euxis::attest::Statement s;
    euxis::attest::Subject subj;
    subj.name = "bench-evidence.tar.gz";
    subj.digest["sha256"] = std::string(64, 'a');
    s.subjects.push_back(subj);
    s.predicate_type = euxis::attest::kSlsaProvenanceV12PredicateType;
    s.predicate = euxis::attest::to_json(euxis::attest::SlsaProvenance{});
    return s;
}

auto tempdir_for(const std::string& tag) -> std::filesystem::path {
    auto p = std::filesystem::temp_directory_path() /
        ("euxis-bench-" + tag + "-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(p);
    return p;
}

// ---- SBOM emission --------------------------------------------------------

void BM_CycloneDX_Emit_5K(benchmark::State& state) {
    auto doc = make_synthetic_sbom(5'000);
    for (auto _ : state) {
        auto json = euxis::sbom::to_cyclonedx_1_6(doc);
        benchmark::DoNotOptimize(json);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * 5'000);
}
BENCHMARK(BM_CycloneDX_Emit_5K)->Unit(benchmark::kMillisecond);

void BM_SPDX_Emit_5K(benchmark::State& state) {
    auto doc = make_synthetic_sbom(5'000);
    for (auto _ : state) {
        auto json = euxis::sbom::to_spdx_3_0_1(doc);
        benchmark::DoNotOptimize(json);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * 5'000);
}
BENCHMARK(BM_SPDX_Emit_5K)->Unit(benchmark::kMillisecond);

void BM_OpenVEX_Emit_100(benchmark::State& state) {
    euxis::sbom::VexDocument vex;
    for (std::size_t i = 0; i < 100; ++i) {
        euxis::sbom::VexStatement st;
        st.vulnerability_name = "CVE-2026-" + std::to_string(i);
        st.product_purls = {"pkg:cargo/pkg-" + std::to_string(i) + "@1"};
        st.status = euxis::sbom::VexStatus::NotAffected;
        st.justification =
            euxis::sbom::VexJustification::VulnerableCodeNotInExecutePath;
        vex.statements.push_back(st);
    }
    for (auto _ : state) {
        auto json = euxis::sbom::to_openvex(vex);
        benchmark::DoNotOptimize(json);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * 100);
}
BENCHMARK(BM_OpenVEX_Emit_100)->Unit(benchmark::kMicrosecond);

// ---- SCA parsers ----------------------------------------------------------

void BM_CargoLock_Parse_1K(benchmark::State& state) {
    auto fixture = make_cargo_lock_fixture(1'000);
    for (auto _ : state) {
        auto parsed = euxis::sca::parse_cargo_lock(fixture);
        benchmark::DoNotOptimize(parsed);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * 1'000);
    state.SetBytesProcessed(
        static_cast<std::int64_t>(state.iterations() * fixture.size()));
}
BENCHMARK(BM_CargoLock_Parse_1K)->Unit(benchmark::kMillisecond);

void BM_NpmLock_Parse_1K(benchmark::State& state) {
    auto fixture = make_npm_lock_fixture(1'000);
    for (auto _ : state) {
        auto parsed = euxis::sca::parse_npm_lock(fixture);
        benchmark::DoNotOptimize(parsed);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * 1'000);
    state.SetBytesProcessed(
        static_cast<std::int64_t>(state.iterations() * fixture.size()));
}
BENCHMARK(BM_NpmLock_Parse_1K)->Unit(benchmark::kMillisecond);

// ---- Slopsquatting --------------------------------------------------------

void BM_Slopsq_Lookup(benchmark::State& state) {
    euxis::slopsquatting::Guard guard;
    guard.load_seed();
    const std::vector<std::string> probes = {
        "lodash", "react", "tokio", "django", "rxjs",
        "easy-fetch", "urlib3", "tokio-utils", "django-rest",
        "pkg-not-in-corpus-12345",
    };
    std::size_t i = 0;
    for (auto _ : state) {
        bool hit = guard.contains(
            probes[i % probes.size()],
            euxis::sbom::PurlType::Npm);
        benchmark::DoNotOptimize(hit);
        ++i;
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_Slopsq_Lookup)->Unit(benchmark::kNanosecond);

// ---- Attestation ----------------------------------------------------------

void BM_DSSE_Sign(benchmark::State& state) {
    auto kp   = euxis::crypto::generate_keypair();
    auto stmt = make_test_statement();
    auto keyid = euxis::attest::derive_keyid(kp.public_key);
    for (auto _ : state) {
        auto env = euxis::attest::sign_statement(stmt, kp.secret_key, keyid);
        benchmark::DoNotOptimize(env);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_DSSE_Sign)->Unit(benchmark::kMicrosecond);

void BM_DSSE_Verify(benchmark::State& state) {
    auto kp   = euxis::crypto::generate_keypair();
    auto stmt = make_test_statement();
    auto env  = *euxis::attest::sign_statement(
        stmt, kp.secret_key,
        euxis::attest::derive_keyid(kp.public_key));
    for (auto _ : state) {
        auto ok = euxis::attest::verify_envelope(env, kp.public_key);
        benchmark::DoNotOptimize(ok);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_DSSE_Verify)->Unit(benchmark::kMicrosecond);

// ---- Cache ----------------------------------------------------------------

void BM_Cache_PutGet(benchmark::State& state) {
    auto dir = tempdir_for("cache");
    auto cache = euxis::cache::ScanCache::open(dir / "c.sqlite");
    if (!cache) {
        state.SkipWithError("ScanCache::open failed");
        return;
    }
    std::size_t i = 0;
    for (auto _ : state) {
        euxis::cache::KeyInputs k{
            .file = "/tmp/x" + std::to_string(i) + ".cpp",
            .content_hash = euxis::cache::hash_string(
                "void f() {" + std::to_string(i) + "}"),
            .ruleset_hash = euxis::cache::hash_string("ruleset-v1"),
            .tool_version = "0.1.0",
        };
        euxis::cache::CacheEntry e;
        e.findings_json = R"({"findings":[],"version":1})";
        auto put = cache->put(k, e);
        benchmark::DoNotOptimize(put);
        auto got = cache->get(euxis::cache::compose_key(k));
        benchmark::DoNotOptimize(got);
        ++i;
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
BENCHMARK(BM_Cache_PutGet)->Unit(benchmark::kMicrosecond);

// ---- File hashing --------------------------------------------------------

void BM_Hash_File_64KiB(benchmark::State& state) {
    auto dir = tempdir_for("hash");
    auto path = dir / "blob.bin";
    {
        std::ofstream f(path, std::ios::binary);
        std::string blob(64 * 1024, 'A');
        f.write(blob.data(), static_cast<std::streamsize>(blob.size()));
    }
    for (auto _ : state) {
        auto h = euxis::cache::hash_file(path);
        benchmark::DoNotOptimize(h);
    }
    state.SetBytesProcessed(
        static_cast<std::int64_t>(state.iterations()) * 64 * 1024);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
BENCHMARK(BM_Hash_File_64KiB)->Unit(benchmark::kMicrosecond);

} // namespace
