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
    # The macOS binary links these from Homebrew (otool -L on v0.0.16).
    depends_on "brotli"
    depends_on "libsodium"
    depends_on "openssl@4"

    # macOS Intel (macos-13) was dropped from the release matrix because
    # GitHub Actions' Intel runner pool is starved; only Apple Silicon
    # tarballs ship today. See .github/workflows/release.yml.
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.16/euxis-v0.0.16-darwin-arm64.tar.gz"
      sha256 "321d86b1a1e083412372e54f8c39e49787d2c58f194ad6d15022e725928a81f8"
    end
  end

  on_linux do
    # Statically linked against musl since v0.0.16: no dependencies.
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.16/euxis-v0.0.16-linux-arm64.tar.gz"
      sha256 "49434f27d8f59a23de023144b8e0c498a225f0b6cc38697a8dc198934ea5a9cb"
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v0.0.16/euxis-v0.0.16-linux-amd64.tar.gz"
      sha256 "77b957e3e6a3fe881f0bfd92a74a14ac425185d5318f9994c369b2400b727dfa"
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
