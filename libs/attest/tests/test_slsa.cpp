#include <gtest/gtest.h>

#include "euxis/attest/slsa.hpp"

namespace euxis::attest {
namespace {

SlsaProvenance make_minimal_provenance() {
    SlsaProvenance p;
    p.build_definition.build_type = "https://euxis.co/attest/scan/v1";
    p.build_definition.external_parameters = {
        {"mode", "triage"},
        {"target", "."},
    };
    p.run_details.metadata.invocation_id = "run-001";
    p.run_details.builder.version["euxis"] = "0.1.0-test";
    return p;
}

TEST(Slsa, JsonHasExpectedTopLevelKeys) {
    auto j = to_json(make_minimal_provenance());
    ASSERT_TRUE(j.contains("buildDefinition"));
    ASSERT_TRUE(j.contains("runDetails"));
    EXPECT_EQ(j["buildDefinition"]["buildType"], "https://euxis.co/attest/scan/v1");
    EXPECT_EQ(j["buildDefinition"]["externalParameters"]["mode"], "triage");
}

TEST(Slsa, RunDetailsCarryBuilderAndMetadata) {
    auto j = to_json(make_minimal_provenance());
    EXPECT_EQ(j["runDetails"]["builder"]["id"], "https://euxis.co/builder");
    EXPECT_EQ(j["runDetails"]["builder"]["version"]["euxis"], "0.1.0-test");
    EXPECT_EQ(j["runDetails"]["metadata"]["invocationId"], "run-001");
    ASSERT_TRUE(j["runDetails"]["metadata"].contains("startedOn"));
    ASSERT_TRUE(j["runDetails"]["metadata"].contains("finishedOn"));
}

TEST(Slsa, ResolvedDependenciesEmitted) {
    SlsaProvenance p = make_minimal_provenance();
    ResolvedDependency dep;
    dep.uri = "git+https://github.com/foo/bar";
    dep.digest["sha256"] = std::string(64, 'b');
    dep.name = "bar";
    p.build_definition.resolved_dependencies.push_back(dep);
    auto j = to_json(p);
    ASSERT_EQ(j["buildDefinition"]["resolvedDependencies"].size(), 1U);
    EXPECT_EQ(j["buildDefinition"]["resolvedDependencies"][0]["uri"],
              "git+https://github.com/foo/bar");
    EXPECT_EQ(j["buildDefinition"]["resolvedDependencies"][0]["digest"]["sha256"],
              std::string(64, 'b'));
}

TEST(Slsa, ByproductsSurfacedWhenPresent) {
    SlsaProvenance p = make_minimal_provenance();
    p.run_details.byproducts.push_back({{"sarif-summary", "10 findings"}});
    auto j = to_json(p);
    ASSERT_TRUE(j["runDetails"].contains("byproducts"));
    EXPECT_EQ(j["runDetails"]["byproducts"][0]["sarif-summary"], "10 findings");
}

TEST(Slsa, MakeProvenanceStatementWrapsCorrectly) {
    Subject subj;
    subj.name = "evidence.tar.gz";
    subj.digest["sha256"] = std::string(64, 'c');
    auto stmt = make_provenance_statement(subj, make_minimal_provenance());
    EXPECT_EQ(stmt.predicate_type,
              std::string{"https://slsa.dev/provenance/v1.2"});
    ASSERT_EQ(stmt.subjects.size(), 1U);
    EXPECT_EQ(stmt.subjects[0].name, "evidence.tar.gz");
    EXPECT_TRUE(stmt.predicate.contains("buildDefinition"));
}

} // namespace
} // namespace euxis::attest
