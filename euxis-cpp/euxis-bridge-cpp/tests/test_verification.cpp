#include <gtest/gtest.h>
#include <euxis/bridge/verification.hpp>
#include <euxis/crypto/ed25519.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>

#include <sodium.h>

namespace euxis::bridge {

class VerificationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { [[maybe_unused]] int rc = sodium_init(); }

    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path() / "euxis_test_verify";
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }
};

TEST_F(VerificationTest, ValidSignature) {
    auto keypair = euxis::crypto::generate_keypair();

    // Write file content
    auto file_path = tmp_dir_ / "skill.js";
    std::string content = "console.log('hello');";
    std::ofstream f(file_path, std::ios::binary);
    f << content;
    f.close();

    // Sign the file
    std::vector<std::byte> msg(content.size());
    std::memcpy(msg.data(), content.data(), content.size());
    auto sig = euxis::crypto::sign(
        std::span<const std::byte, 64>{keypair.secret_key},
        std::span<const std::byte>{msg}
    );

    // Write signature
    auto sig_path = tmp_dir_ / "skill.js.sig";
    std::ofstream sf(sig_path, std::ios::binary);
    sf.write(reinterpret_cast<const char*>(sig.data()), sig.size());
    sf.close();

    auto result = verify_skill_signature(file_path, sig_path, keypair.public_key);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->valid);
}

TEST_F(VerificationTest, InvalidSignature) {
    auto keypair = euxis::crypto::generate_keypair();
    auto other_keypair = euxis::crypto::generate_keypair();

    auto file_path = tmp_dir_ / "skill.js";
    std::string content = "console.log('hello');";
    std::ofstream f(file_path, std::ios::binary);
    f << content;
    f.close();

    // Sign with different key
    std::vector<std::byte> msg(content.size());
    std::memcpy(msg.data(), content.data(), content.size());
    auto sig = euxis::crypto::sign(
        std::span<const std::byte, 64>{other_keypair.secret_key},
        std::span<const std::byte>{msg}
    );

    auto sig_path = tmp_dir_ / "skill.js.sig";
    std::ofstream sf(sig_path, std::ios::binary);
    sf.write(reinterpret_cast<const char*>(sig.data()), sig.size());
    sf.close();

    auto result = verify_skill_signature(file_path, sig_path, keypair.public_key);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->valid);
}

TEST_F(VerificationTest, MissingFile) {
    std::array<std::byte, 32> key{};
    auto result = verify_skill_signature(
        tmp_dir_ / "nonexistent.js",
        tmp_dir_ / "nonexistent.sig",
        key
    );
    EXPECT_FALSE(result.has_value());
}

TEST_F(VerificationTest, LoadPublicKeyRaw) {
    auto keypair = euxis::crypto::generate_keypair();
    auto key_path = tmp_dir_ / "key.pub";

    std::ofstream f(key_path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(keypair.public_key.data()), 32);
    f.close();

    auto result = load_public_key(key_path);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, keypair.public_key);
}

}  // namespace euxis::bridge
