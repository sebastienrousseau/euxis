# typed: false
# frozen_string_literal: true

# Homebrew formula for the `euxis` CLI from
# https://github.com/sebastienrousseau/euxis
#
# Updated by hand per release: the three `url` lines and their `sha256`
# from the release's SHA256SUMS. The version is read from the URL.
#
# Verify locally with:
#   brew install --build-from-source euxis.rb && euxis --version

class Euxis < Formula
  desc "Ultra-fast open-source code security scanner"
  homepage "https://euxis.co"
  license "AGPL-3.0-only"

  on_macos do
    # The macOS binary links these from Homebrew (otool -L on v0.0.14).
    depends_on "brotli"
    depends_on "libsodium"
    depends_on "openssl@4"

    # macOS Intel (macos-13) was dropped from the release matrix because
    # GitHub Actions' Intel runner pool is starved; only Apple Silicon
    # tarballs ship today. See .github/workflows/release.yml.
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.14/euxis-v0.0.14-darwin-arm64.tar.gz"
      sha256 "2dee5be1c91262e0552bce6b8531bcca7c92a40f3cb9016a6857633a072f3054"
    end
  end

  on_linux do
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.14/euxis-v0.0.14-linux-arm64.tar.gz"
      sha256 "57d53bf39c6ce29958274f1899924e507bdcdb8c82e9a2e1ee814220cd1530de"
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.14/euxis-v0.0.14-linux-amd64.tar.gz"
      sha256 "92d345b2cdb09ce85538023a8610cc96ef0b15a6422be80799488ed034489ce7"
    end
  end

  def install
    # Release tarballs contain a single binary named
    # `euxis-v<version>-<target>`. Rename to `euxis` and drop it
    # into Homebrew's managed bin/ prefix.
    bin_name = Dir["euxis-*"].first
    raise "euxis binary not found in tarball" unless bin_name

    bin.install bin_name => "euxis"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/euxis --version")
  end
end
