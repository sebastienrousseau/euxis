#include <gtest/gtest.h>

#include "euxis/attest/statement.hpp"

namespace euxis::attest {
namespace {

Statement make_minimal_statement() {
    Statement s;
    Subject subj;
    subj.name = "euxis-evidence.tar.gz";
    subj.digest["sha256"] = std::string(64, 'a');
    s.subjects.push_back(subj);
    s.predicate_type = "https://slsa.dev/provenance/v1.2";
    s.predicate      = nlohmann::json::object();
    return s;
}

TEST(Statement, JsonShapeMatchesInTotoV1) {
    auto s = make_minimal_statement();
    auto j = to_json(s);
    EXPECT_EQ(j["_type"], "https://in-toto.io/Statement/v1");
    EXPECT_EQ(j["predicateType"], "https://slsa.dev/provenance/v1.2");
    ASSERT_TRUE(j.contains("subject"));
    ASSERT_TRUE(j["subject"].is_array());
    EXPECT_EQ(j["subject"][0]["name"], "euxis-evidence.tar.gz");
    EXPECT_EQ(j["subject"][0]["digest"]["sha256"], std::string(64, 'a'));
}

TEST(Statement, ValidateAcceptsMinimal) {
    EXPECT_TRUE(validate(make_minimal_statement()).has_value());
}

TEST(Statement, ValidateRejectsEmptySubjects) {
    Statement s = make_minimal_statement();
    s.subjects.clear();
    EXPECT_FALSE(validate(s).has_value());
}

TEST(Statement, ValidateRejectsEmptyName) {
    Statement s = make_minimal_statement();
    s.subjects[0].name.clear();
    EXPECT_FALSE(validate(s).has_value());
}

TEST(Statement, ValidateRejectsEmptyDigest) {
    Statement s = make_minimal_statement();
    s.subjects[0].digest.clear();
    EXPECT_FALSE(validate(s).has_value());
}

TEST(Statement, ValidateRejectsNonHexDigest) {
    Statement s = make_minimal_statement();
    s.subjects[0].digest["sha256"] = "not_hex_zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    EXPECT_FALSE(validate(s).has_value());
}

TEST(Statement, ValidateRejectsMissingPredicateType) {
    Statement s = make_minimal_statement();
    s.predicate_type.clear();
    EXPECT_FALSE(validate(s).has_value());
}

TEST(Statement, ValidateRejectsNonUriPredicateType) {
    Statement s = make_minimal_statement();
    s.predicate_type = "not-a-uri";
    EXPECT_FALSE(validate(s).has_value());
}

TEST(Statement, SerialiseIsDeterministicGivenSameInput) {
    auto s = make_minimal_statement();
    EXPECT_EQ(serialise_for_signing(s), serialise_for_signing(s));
}

} // namespace
} // namespace euxis::attest
