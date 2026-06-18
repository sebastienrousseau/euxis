# typed: false
# frozen_string_literal: true

# Homebrew formula for the `euxis` CLI from
# https://github.com/sebastienrousseau/euxis
#
# This file is the template the release pipeline rewrites per tag:
# .github/workflows/release.yml emits per-platform tarballs at
#   https://github.com/sebastienrousseau/euxis/releases/download/<tag>/
# and a small bumper Action in the tap repo swaps the four `sha256`
# lines below before opening a PR against sebastienrousseau/homebrew-tap.
#
# Verify locally with:
#   brew install --build-from-source euxis.rb && euxis --version

class Euxis < Formula
  desc 'Ultra-fast open-source code security scanner'
  homepage 'https://euxis.co'
  license 'AGPL-3.0-only'
  version '0.1.3'

  on_macos do
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-darwin-arm64.tar.gz"
      sha256 'REPLACE_WITH_darwin-arm64_SHA256'
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-darwin-amd64.tar.gz"
      sha256 'REPLACE_WITH_darwin-amd64_SHA256'
    end
  end

  on_linux do
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-linux-arm64.tar.gz"
      sha256 'REPLACE_WITH_linux-arm64_SHA256'
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-linux-amd64.tar.gz"
      sha256 'REPLACE_WITH_linux-amd64_SHA256'
    end
  end

  def install
    # Release tarballs contain a single binary named
    # `euxis-v<version>-<target>`. Rename to `euxis` and drop it
    # into Homebrew's managed bin/ prefix.
    bin_name = Dir['euxis-*'].first
    raise 'euxis binary not found in tarball' unless bin_name

    bin.install bin_name => 'euxis'
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/euxis --version")
  end
end
