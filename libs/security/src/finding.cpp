#include "euxis/security/finding.hpp"

#include <algorithm>
#include <cctype>

namespace euxis::security {

auto owasp_id(OwaspCategory c) noexcept -> const char* {
    switch (c) {
        case OwaspCategory::A01_BrokenAccessControl:                 return "A01:2025";
        case OwaspCategory::A02_CryptographicFailures:               return "A02:2025";
        case OwaspCategory::A03_SupplyChainFailures:                 return "A03:2025";
        case OwaspCategory::A04_InsecureDesign:                      return "A04:2025";
        case OwaspCategory::A05_SecurityMisconfiguration:            return "A05:2025";
        case OwaspCategory::A06_VulnerableComponents:                return "A06:2025";
        case OwaspCategory::A07_AuthenticationFailures:              return "A07:2025";
        case OwaspCategory::A08_DataIntegrityFailures:               return "A08:2025";
        case OwaspCategory::A09_LoggingMonitoringFailures:           return "A09:2025";
        case OwaspCategory::A10_MishandlingExceptionalConditions:    return "A10:2025";
        case OwaspCategory::None: break;
    }
    return "";
}

auto owasp_label(OwaspCategory c) noexcept -> const char* {
    switch (c) {
        case OwaspCategory::A01_BrokenAccessControl:                 return "Broken Access Control";
        case OwaspCategory::A02_CryptographicFailures:               return "Cryptographic Failures";
        case OwaspCategory::A03_SupplyChainFailures:                 return "Software Supply Chain Failures";
        case OwaspCategory::A04_InsecureDesign:                      return "Insecure Design";
        case OwaspCategory::A05_SecurityMisconfiguration:            return "Security Misconfiguration";
        case OwaspCategory::A06_VulnerableComponents:                return "Vulnerable and Outdated Components";
        case OwaspCategory::A07_AuthenticationFailures:              return "Identification and Authentication Failures";
        case OwaspCategory::A08_DataIntegrityFailures:               return "Software and Data Integrity Failures";
        case OwaspCategory::A09_LoggingMonitoringFailures:           return "Security Logging and Monitoring Failures";
        case OwaspCategory::A10_MishandlingExceptionalConditions:    return "Mishandling of Exceptional Conditions";
        case OwaspCategory::None: break;
    }
    return "";
}

auto quantum_label(QuantumDeprecation q) noexcept -> const char* {
    switch (q) {
        case QuantumDeprecation::Rsa:               return "RSA (quantum-vulnerable; CNSA 2.0 deprecation 2030)";
        case QuantumDeprecation::Ecdsa:             return "ECDSA (quantum-vulnerable; CNSA 2.0 deprecation 2030)";
        case QuantumDeprecation::Ecdh:              return "ECDH (quantum-vulnerable; CNSA 2.0 deprecation 2030)";
        case QuantumDeprecation::DhFiniteField:     return "Finite-field DH (quantum-vulnerable; CNSA 2.0 deprecation 2030)";
        case QuantumDeprecation::Sha1InSignature:   return "SHA-1 in signature context (collision-broken)";
        case QuantumDeprecation::Md5InSignature:    return "MD5 in signature context (collision-broken)";
        case QuantumDeprecation::NonHybridKemMlKem: return "ML-KEM used non-hybridised (transitional warning)";
        case QuantumDeprecation::NonHybridSigMlDsa: return "ML-DSA used non-hybridised (transitional warning)";
        case QuantumDeprecation::None: break;
    }
    return "";
}

auto parse_severity(const std::string& s) -> Severity {
    std::string lower;
    lower.reserve(s.size());
    for (char c : s) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lower == "critical") return Severity::Critical;
    if (lower == "high")     return Severity::High;
    if (lower == "medium")   return Severity::Medium;
    if (lower == "low")      return Severity::Low;
    if (lower == "info")     return Severity::Info;
    if (lower == "none")     return Severity::None;
    return Severity::None;
}

} // namespace euxis::security
