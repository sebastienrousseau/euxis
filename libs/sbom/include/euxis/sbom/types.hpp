/// @file
/// @brief Canonical SBOM intermediate representation.
///
/// Every code path that needs to produce an SBOM (CycloneDX 1.6,
/// SPDX 3.0.1, OpenVEX exception document) starts from these types.
/// The downstream emitters in cyclonedx.hpp / spdx.hpp / openvex.hpp
/// are pure functions that take an `SbomDocument` and return JSON.
///
/// This separation means a single scan can produce all three formats
/// from one in-memory pass over the dependency graph; CRA-compliant
/// CI workflows can attach both CycloneDX (cyclonedx-cli) and SPDX
/// (federal procurement) without reparsing manifests.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace euxis::sbom {

/// Component classification per CycloneDX 1.6 §componentType.
enum class ComponentType : std::uint8_t {
    Application,
    Framework,
    Library,
    Container,
    Platform,
    OperatingSystem,
    Device,
    Firmware,
    File,
    /// Foundation-model / weights artefact (CycloneDX 1.6 ML/AI).
    MachineLearningModel,
    /// Dataset bundled with a model.
    Data,
};

/// Cryptographic hash algorithm used to fingerprint a component.
enum class HashAlgorithm : std::uint8_t {
    Md5,
    Sha1,
    Sha256,
    Sha384,
    Sha512,
    Sha3_256,
    Sha3_512,
    Blake2b_256,
    Blake2b_512,
    Blake3,
};

struct Hash {
    HashAlgorithm algorithm{HashAlgorithm::Sha256};
    /// Hex-encoded digest. Length validated by `is_valid_hash()`.
    std::string value;
};

/// SPDX licence expression. Either a single licence id ("MIT") or a
/// boolean expression ("(MIT OR Apache-2.0)"). The full SPDX 3.0.1
/// grammar is not enforced; downstream emitters pass the string
/// through verbatim into the `expression` field.
struct License {
    std::string spdx_expression;
    std::optional<std::string> url;
};

/// External reference per CycloneDX 1.6 §externalReference.
enum class ExternalRefType : std::uint8_t {
    Vcs,
    IssueTracker,
    Website,
    Advisories,
    Bom,
    MailingList,
    SocialMedia,
    Chat,
    Documentation,
    Support,
    Distribution,
    DistributionIntake,
    License,
    BuildMeta,
    BuildSystem,
    ReleaseNotes,
    Other,
};

struct ExternalRef {
    ExternalRefType type{ExternalRefType::Other};
    std::string url;
    std::string comment;
};

/// A single component (application, library, file, model, …) in the
/// dependency graph. Stable across emitters.
struct Component {
    /// Unique within the SbomDocument. Used as the SPDX SPDXID and
    /// as the CycloneDX bom-ref. Must be deterministic across
    /// reproducible builds.
    std::string bom_ref;

    /// purl per RFC 9 (Package URL spec). See purl.hpp.
    /// Empty when the component is not addressable via purl (e.g.
    /// vendored source).
    std::string purl;

    /// Component name (e.g. "tokio", "react"). Required.
    std::string name;

    /// Version string verbatim from the manifest. CycloneDX 1.6 does
    /// not enforce SemVer; SPDX 3.0.1 takes the string as-is.
    std::string version;

    ComponentType type{ComponentType::Library};

    /// Optional supplier / publisher; populated when known.
    std::string supplier;
    std::string publisher;
    std::string description;

    std::vector<License> licenses;
    std::vector<Hash> hashes;
    std::vector<ExternalRef> external_refs;

    /// CPE 2.3 string when known. CISA's NVD / KEV catalogues match
    /// on this; useful for OSV.dev fallback when purl is absent.
    std::string cpe;

    /// CycloneDX 1.6 properties — generic key/value bag. Used for
    /// "euxis:reachable" flag and "euxis:direct" vs transitive
    /// classification.
    std::unordered_map<std::string, std::string> properties;
};

/// Single edge in the dependency graph: `ref` depends on each entry
/// in `depends_on`.
struct Dependency {
    std::string ref;
    std::vector<std::string> depends_on;
};

/// Tool that produced the SBOM. Embedded in CycloneDX `metadata.tools`
/// and SPDX `creationInfo.tool`.
struct Tool {
    std::string vendor;
    std::string name;
    std::string version;
};

/// The canonical document. Every field is optional except `components`;
/// each emitter handles missing data sensibly.
struct SbomDocument {
    /// Reverse-DNS-style namespace for the BOM identifier.
    std::string document_namespace{"https://euxis.co/sbom"};

    /// Stable identifier; defaults to a UUIDv4 chosen at emit time
    /// if empty.
    std::string serial_number;

    /// Component representing the project being scanned. Listed in
    /// CycloneDX `metadata.component` and as the SPDX root package.
    std::optional<Component> root;

    std::vector<Component> components;
    std::vector<Dependency> dependencies;
    std::vector<Tool> tools;

    /// When the document was created.
    std::chrono::system_clock::time_point created_at{
        std::chrono::system_clock::now()};

    /// Free-form metadata properties, surfaced under CycloneDX
    /// `metadata.properties` and SPDX `creationInfo.comment`.
    std::unordered_map<std::string, std::string> properties;
};

// ---- helpers ----------------------------------------------------------------

/// Stringify ComponentType for CycloneDX `type` field.
[[nodiscard]] auto component_type_str(ComponentType t) noexcept -> const char*;

/// Stringify HashAlgorithm for CycloneDX `alg` and SPDX
/// `checksumAlgorithm`. CycloneDX uses "SHA-256"; SPDX uses
/// "SHA256". Use the appropriate accessor.
[[nodiscard]] auto hash_alg_cyclonedx(HashAlgorithm a) noexcept -> const char*;
[[nodiscard]] auto hash_alg_spdx(HashAlgorithm a) noexcept -> const char*;

/// Expected hex digest length for a given algorithm. Returns 0 for
/// algorithms with variable-length output (e.g. BLAKE3).
[[nodiscard]] constexpr auto digest_hex_length(HashAlgorithm a) noexcept -> std::size_t {
    switch (a) {
        case HashAlgorithm::Md5:        return 32;
        case HashAlgorithm::Sha1:       return 40;
        case HashAlgorithm::Sha256:     return 64;
        case HashAlgorithm::Sha384:     return 96;
        case HashAlgorithm::Sha512:     return 128;
        case HashAlgorithm::Sha3_256:   return 64;
        case HashAlgorithm::Sha3_512:   return 128;
        case HashAlgorithm::Blake2b_256:return 64;
        case HashAlgorithm::Blake2b_512:return 128;
        case HashAlgorithm::Blake3:     return 0;  // variable length
    }
    return 0;
}

/// True when `hex` is the expected length for `alg` and contains
/// only hex characters.
[[nodiscard]] auto is_valid_hash(const Hash& hash) noexcept -> bool;

/// Stringify ExternalRefType per CycloneDX 1.6 vocab.
[[nodiscard]] auto external_ref_type_str(ExternalRefType t) noexcept -> const char*;

/// Generate a deterministic UUIDv4-like serial-number string for a
/// document. Used when `SbomDocument::serial_number` is empty.
[[nodiscard]] auto generate_serial_number() -> std::string;

/// Format a time point as RFC 3339 (the format CycloneDX and SPDX
/// both expect for `timestamp` / `created`).
[[nodiscard]] auto to_rfc3339(std::chrono::system_clock::time_point tp) -> std::string;

} // namespace euxis::sbom
