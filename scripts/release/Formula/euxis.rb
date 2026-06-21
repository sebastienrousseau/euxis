# typed: false
# frozen_string_literal: true

# Homebrew formula for the `euxis` CLI from
# https://github.com/sebastienrousseau/euxis
#
# Updated per release by the bumper Action in the tap repo (it rewrites
# the `version` line and the per-platform `sha256` lines).
#
# Verify locally with:
#   brew install --build-from-source euxis.rb && euxis --version

class Euxis < Formula
  desc 'Ultra-fast open-source code security scanner'
  homepage 'https://euxis.co'
  license 'AGPL-3.0-only'
  version '0.1.3'

  on_macos do
    # macOS Intel (macos-13) was dropped from the release matrix because
    # GitHub Actions' Intel runner pool is starved; only Apple Silicon
    # tarballs ship today. See .github/workflows/release.yml.
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-darwin-arm64.tar.gz"
      sha256 'c596155859215e386a0aff5125914fe28fc66ac05a96edbbb9806d23b544d251'
    end
  end

  on_linux do
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-linux-arm64.tar.gz"
      sha256 'b813765647a9de85934ed95a1e8ad2b6634db9e434fa1576650e8aa016cb961e'
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-linux-amd64.tar.gz"
      sha256 '05eaad3775e59b586096f4c28c517ed0d9f05592d68a842232213c5daa180fbc'
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
