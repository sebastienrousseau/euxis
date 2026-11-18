#include "euxis/crypto/xchacha.hpp"

#include <gtest/gtest.h>
#include <sodium.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace euxis::crypto {
namespace {

class XChaChaTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_GE(sodium_init(), 0); }

    static auto random_key() -> std::array<std::byte, 32> {
        std::array<std::byte, 32> key{};
        randombytes_buf(key.data(), key.size());
        return key;
    }

    static auto bytes(const std::string& s) -> std::vector<std::byte> {
        std::vector<std::byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }
};

TEST_F(XChaChaTest, RoundTrip) {
    const auto key = random_key();
    const auto pt = bytes("memory entry");
    auto box = xchacha_encrypt(pt, key);
    ASSERT_TRUE(box.has_value()) << to_string(box.error());
    EXPECT_EQ(box->ciphertext.size(), pt.size() + kXChaChaTagBytes);
    auto out = xchacha_decrypt(box->ciphertext, key, box->nonce);
    ASSERT_TRUE(out.has_value()) << to_string(out.error());
    EXPECT_EQ(out->plaintext, pt);
    EXPECT_EQ(out->algorithm, kXChaChaAlgorithm);
}

TEST_F(XChaChaTest, RoundTripWithAad) {
    const auto key = random_key();
    const auto pt = bytes("secret");
    const auto aad = bytes("hot");
    auto box = xchacha_encrypt(pt, key, aad);
    ASSERT_TRUE(box.has_value());
    auto out = xchacha_decrypt(box->ciphertext, key, box->nonce, aad);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->plaintext, pt);
}

TEST_F(XChaChaTest, EmptyPlaintext) {
    const auto key = random_key();
    auto box = xchacha_encrypt({}, key);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(box->ciphertext.size(), kXChaChaTagBytes);
    auto out = xchacha_decrypt(box->ciphertext, key, box->nonce);
    ASSERT_TRUE(out.has_value());
    EXPECT_TRUE(out->plaintext.empty());
}

TEST_F(XChaChaTest, WrongAadFails) {
    const auto key = random_key();
    auto box = xchacha_encrypt(bytes("x"), key, bytes("hot"));
    ASSERT_TRUE(box.has_value());
    auto out = xchacha_decrypt(box->ciphertext, key, box->nonce, bytes("cold"));
    ASSERT_FALSE(out.has_value());
    EXPECT_EQ(out.error(), CryptoError::AuthenticationFailed);
}

TEST_F(XChaChaTest, MissingAadFails) {
    const auto key = random_key();
    auto box = xchacha_encrypt(bytes("x"), key, bytes("hot"));
    ASSERT_TRUE(box.has_value());
    EXPECT_FALSE(xchacha_decrypt(box->ciphertext, key, box->nonce).has_value());
}

TEST_F(XChaChaTest, WrongKeyFails) {
    auto box = xchacha_encrypt(bytes("x"), random_key());
    ASSERT_TRUE(box.has_value());
    EXPECT_FALSE(xchacha_decrypt(box->ciphertext, random_key(), box->nonce).has_value());
}

TEST_F(XChaChaTest, TamperedCiphertextFails) {
    const auto key = random_key();
    auto box = xchacha_encrypt(bytes("payload"), key);
    ASSERT_TRUE(box.has_value());
    box->ciphertext[0] ^= std::byte{0x01};
    EXPECT_FALSE(xchacha_decrypt(box->ciphertext, key, box->nonce).has_value());
}

TEST_F(XChaChaTest, TamperedNonceFails) {
    const auto key = random_key();
    auto box = xchacha_encrypt(bytes("payload"), key);
    ASSERT_TRUE(box.has_value());
    box->nonce[23] ^= std::byte{0x80};
    EXPECT_FALSE(xchacha_decrypt(box->ciphertext, key, box->nonce).has_value());
}

TEST_F(XChaChaTest, NoncesAreUnique) {
    const auto key = random_key();
    auto a = xchacha_encrypt(bytes("same"), key);
    auto b = xchacha_encrypt(bytes("same"), key);
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_NE(a->nonce, b->nonce);
    EXPECT_NE(a->ciphertext, b->ciphertext);
}

TEST_F(XChaChaTest, ShortCiphertextIsRejected) {
    const auto key = random_key();
    std::array<std::byte, kXChaChaNonceBytes> nonce{};
    const std::vector<std::byte> short_ct(kXChaChaTagBytes - 1);
    auto out = xchacha_decrypt(short_ct, key, nonce);
    ASSERT_FALSE(out.has_value());
    EXPECT_EQ(out.error(), CryptoError::DecryptionFailed);
}

TEST_F(XChaChaTest, ZeroKeyIsRejected) {
    const std::array<std::byte, 32> zero{};
    auto box = xchacha_encrypt(bytes("x"), zero);
    ASSERT_FALSE(box.has_value());
    EXPECT_EQ(box.error(), CryptoError::InvalidInput);
}

// RFC draft-irtf-cfrg-xchacha-03 §A.3.1 test vector: proves this is the
// standard construction, not just self-consistent.
TEST_F(XChaChaTest, MatchesRfcDraftVector) {
    auto hex = [](std::string_view h) {
        std::vector<std::byte> out(h.size() / 2);
        size_t len = 0;
        EXPECT_EQ(sodium_hex2bin(reinterpret_cast<unsigned char*>(out.data()), out.size(),
                                 h.data(), h.size(), nullptr, &len, nullptr), 0);
        out.resize(len);
        return out;
    };
    const auto pt = bytes(
        "Ladies and Gentlemen of the class of '99: If I could offer you only one tip "
        "for the future, sunscreen would be it.");
    const auto aad = hex("50515253c0c1c2c3c4c5c6c7");
    const auto key_v = hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    const auto nonce_v = hex("404142434445464748494a4b4c4d4e4f5051525354555657");
    const auto expected = hex(
        "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb"
        "731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b452"
        "2f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff9"
        "21f9664c97637da9768812f615c68b13b52e"
        "c0875924c1c7987947deafd8780acf49");

    std::array<std::byte, 32> key{};
    std::memcpy(key.data(), key_v.data(), key.size());
    std::array<std::byte, kXChaChaNonceBytes> nonce{};
    std::memcpy(nonce.data(), nonce_v.data(), nonce.size());

    auto out = xchacha_decrypt(expected, key, nonce, aad);
    ASSERT_TRUE(out.has_value()) << to_string(out.error());
    EXPECT_EQ(out->plaintext, pt);
}

} // namespace
} // namespace euxis::crypto
