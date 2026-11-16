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
  version '0.0.12'

  on_macos do
    # macOS Intel (macos-13) was dropped from the release matrix because
    # GitHub Actions' Intel runner pool is starved; only Apple Silicon
    # tarballs ship today. See .github/workflows/release.yml.
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-darwin-arm64.tar.gz"
      sha256 '049c2e23da8ee07c3990060ae5c830c9f6ea7ca249ed69660a19ebcae7770d2f'
    end
  end

  on_linux do
    on_arm do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-linux-arm64.tar.gz"
      sha256 '335c17cef71d0bf18ca2c617578cd1b212daadb8937106eb832b39e8498c3e67'
    end
    on_intel do
      url "https://github.com/sebastienrousseau/euxis/releases/download/v#{version}/euxis-v#{version}-linux-amd64.tar.gz"
      sha256 '467c9adf6802df50d932e89ec5778228917ac1c1b1d4b1dcec5dc2de81e60ae4'
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
