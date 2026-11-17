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
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.15/euxis-v0.0.15-darwin-arm64.tar.gz"
      sha256 "586bd754fb9934f0f26c8a4fa1c6dd99f2068e7002610e9efe2fbe6ce7086105"
    end
  end

  on_linux do
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.15/euxis-v0.0.15-linux-arm64.tar.gz"
      sha256 "b5e5f856741bd916ba39509d8c2cab7d5510bd06f0f527e2eaa14b3ec7e463c9"
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.15/euxis-v0.0.15-linux-amd64.tar.gz"
      sha256 "ec7a5ee253635213a277348d898e69b1d8febda16809a5b53dbfeb9c6230d305"
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
