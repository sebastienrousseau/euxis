# Euxis Cross-Platform Verification System

## Overview

Comprehensive automation system ensuring Euxis v0.0.6 build/package/runtime parity across macOS, Linux, and WSL platforms.

## Verification Architecture

### 1. Automated CI/CD Pipeline
**File:** `.github/workflows/cross-platform-ci.yml`
- **Platform Matrix:** Ubuntu (Linux), macOS, Windows (WSL)
- **Python Version:** 3.12
- **Triggers:** Push, PR, manual dispatch
- **Gates:** Environment setup, dependency installation, script verification, runtime testing

### 2. Local Verification Tool
**File:** `bin/euxis-cross-platform-verify`
- **Usage:** `euxis-cross-platform-verify [--local-only] [--version VERSION]`
- **Capabilities:** Platform detection, dependency validation, script testing
- **Integration:** Can trigger GitHub Actions workflow for full 3-platform verification

### 3. Multi-Platform Container Build
**Files:**
- `deploy/Dockerfile.multi-platform` - Multi-architecture container build
- `deploy/docker-compose.multi-platform.yml` - Orchestration configuration
- `deploy/build-multi-platform.sh` - Automated build and verification script

## Verification Gates

### Gate 1: Python Environment ✅
- Python 3.11+ requirement validation
- Version compatibility check
- Platform-specific Python path verification

### Gate 2: Dependencies ✅
- chromadb installation and import test
- sentence-transformers installation and import test
- Automatic installation attempt for missing dependencies

### Gate 3: Core Scripts ✅
- Executable permission verification for all `bin/euxis*` scripts
- Script count validation (33+ scripts expected)
- Basic functionality test with timeout protection

### Gate 4: Agent Registry ✅
- JSON validity check for `registry.json`
- Agent count verification (6+ agents expected)
- Schema validation

### Gate 5: Integration Tests ✅
- File I/O operations (create, read, delete)
- Shell command execution
- Basic system integration verification

## Usage Examples

### Local Platform Verification
```bash
# Test current platform only
./bin/euxis-cross-platform-verify --local-only

# Test specific version
./bin/euxis-cross-platform-verify --version v0.0.7 --local-only
```

### Full Cross-Platform CI
```bash
# Trigger GitHub Actions workflow (requires gh CLI)
./bin/euxis-cross-platform-verify --version v0.0.6

# Manual GitHub workflow trigger
gh workflow run cross-platform-ci.yml --field version=v0.0.6
```

### Multi-Platform Container Build
```bash
# Build for AMD64 and ARM64
./deploy/build-multi-platform.sh --version v0.0.6

# Build and push to registry
./deploy/build-multi-platform.sh --push --registry ghcr.io/euxis

# Build specific platforms only
./deploy/build-multi-platform.sh --platforms linux/amd64 --no-verify
```

## Platform-Specific Considerations

### macOS
- Uses Homebrew for `portaudio` dependency
- Native bash and Python 3.12 from system/Homebrew
- File path handling with BSD utilities

### Linux
- APT package manager for system dependencies
- Native bash and Python from package manager
- GNU utilities (readlink, realpath)

### WSL (Windows Subsystem for Linux)
- Ubuntu 22.04 distribution
- APT package management within WSL environment
- Windows/Linux path translation handled automatically

## Security & Best Practices

### Container Security
- Non-root user execution (`euxis` user)
- Minimal base image (python:3.11-slim)
- Health checks and resource constraints
- Multi-stage builds for optimization

### CI/CD Security
- Secrets handling via GitHub Secrets
- No hardcoded credentials in workflows
- Fail-fast strategy for quick feedback
- Platform isolation (separate runners)

## Monitoring & Reporting

### Automated Reports
- Platform verification reports generated per run
- Consolidated cross-platform summary
- Build artifacts uploaded to GitHub Actions
- Health status badges (planned)

### Metrics Tracked
- Build success/failure rates per platform
- Verification gate pass/fail statistics
- Dependency installation timing
- Script execution performance

## Integration with Release Pipeline

This verification system integrates with the Euxis release pipeline:

1. **Gate 3** of `euxis-verify-all` calls automation-engineer
2. **Release readiness** requires all platforms to pass verification
3. **Docker images** built for multi-architecture deployment
4. **CI/CD integration** ensures no platform regressions

## Troubleshooting

### Common Issues

**Python Version Mismatch**
```bash
# Ensure Python 3.11+ is installed
python3 --version
# Update Python if needed
```

**Missing Dependencies**
```bash
# Manual dependency installation
pip install chromadb sentence-transformers
```

**Permission Denied**
```bash
# Fix script permissions
chmod +x bin/euxis*
```

**Container Build Failures**
```bash
# Check Docker buildx
docker buildx ls
# Create builder if needed
docker buildx create --name multi-platform --use
```

## Version Compatibility

| Euxis Version | Python | Platforms | Docker |
|---------------|--------|-----------|--------|
| v0.0.6        | 3.11+  | macOS, Linux, WSL | 20.10+ |
| v0.0.7+       | 3.11+  | macOS, Linux, WSL, ARM64 | 20.10+ |

## Future Enhancements

- [ ] ARM64 macOS (Apple Silicon) native testing
- [ ] Performance benchmarking across platforms
- [ ] Automated dependency vulnerability scanning
- [ ] Platform-specific optimization profiles
- [ ] Integration with package managers (homebrew, apt, chocolatey)

---

**Verification Status for v0.0.6:** ✅ IMPLEMENTED
**Last Updated:** 2026-02-01
**Next Review:** Release candidate testing

> Designed by **Sebastien Rousseau**
> Engineered with **[Euxis](https://euxis.co/)** — Enterprise Unified eXecution Intelligence System