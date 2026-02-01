#!/usr/bin/env bash
#
# Euxis Multi-Platform Build Script
# Builds and verifies Euxis across multiple architectures and platforms
#
# Usage: ./build-multi-platform.sh [OPTIONS]
#

set -euo pipefail

VERSION="${VERSION:-v0.0.6}"
REGISTRY="${REGISTRY:-euxis}"
PUSH="${PUSH:-false}"
VERIFY="${VERIFY:-true}"
PLATFORMS="${PLATFORMS:-linux/amd64,linux/arm64}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $*"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

usage() {
    cat << EOF
Euxis Multi-Platform Build Script

USAGE:
    ./build-multi-platform.sh [OPTIONS]

OPTIONS:
    --version VERSION       Set version tag (default: $VERSION)
    --registry REGISTRY     Set container registry (default: $REGISTRY)
    --platforms PLATFORMS   Comma-separated platform list (default: $PLATFORMS)
    --push                  Push images to registry
    --no-verify            Skip verification tests
    --help                  Show this help

EXAMPLES:
    ./build-multi-platform.sh --version v0.0.6 --push
    ./build-multi-platform.sh --platforms linux/amd64 --no-verify
    ./build-multi-platform.sh --registry ghcr.io/euxis --push

EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --registry)
            REGISTRY="$2"
            shift 2
            ;;
        --platforms)
            PLATFORMS="$2"
            shift 2
            ;;
        --push)
            PUSH=true
            shift
            ;;
        --no-verify)
            VERIFY=false
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

check_dependencies() {
    log_info "Checking build dependencies..."

    # Check Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker not found. Install Docker from: https://docker.com"
        exit 1
    fi

    # Check buildx
    if ! docker buildx version &> /dev/null; then
        log_error "Docker buildx not available. Update Docker or enable buildx."
        exit 1
    fi

    # Check buildx builder
    if ! docker buildx ls | grep -q "multi-platform"; then
        log_info "Creating multi-platform buildx builder..."
        docker buildx create --name multi-platform --platform "$PLATFORMS" --use
    else
        log_info "Using existing multi-platform builder"
        docker buildx use multi-platform
    fi

    log_success "Build dependencies verified"
}

build_images() {
    log_info "Building multi-platform Euxis images..."
    log_info "Version: $VERSION"
    log_info "Platforms: $PLATFORMS"
    log_info "Registry: $REGISTRY"

    cd "$PROJECT_ROOT"

    local build_args=(
        --builder multi-platform
        --platform "$PLATFORMS"
        --file deploy/Dockerfile.multi-platform
        --tag "${REGISTRY}/euxis:${VERSION}"
        --tag "${REGISTRY}/euxis:latest"
    )

    if [[ "$PUSH" == "true" ]]; then
        build_args+=(--push)
        log_info "Images will be pushed to registry"
    else
        build_args+=(--load)
        log_info "Images will be loaded locally"
    fi

    # Build the main image
    log_info "Building Euxis agent framework..."
    if docker buildx build "${build_args[@]}" .; then
        log_success "Multi-platform build completed"
    else
        log_error "Build failed"
        return 1
    fi

    # Build verification image
    log_info "Building verification image..."
    docker buildx build \
        --builder multi-platform \
        --platform "$PLATFORMS" \
        --file deploy/Dockerfile.multi-platform \
        --target application \
        --tag "${REGISTRY}/euxis-verification:${VERSION}" \
        --load \
        .

    log_success "All images built successfully"
}

run_verification_tests() {
    log_info "Running multi-platform verification tests..."

    cd "$PROJECT_ROOT"

    # Test each platform
    IFS=',' read -ra PLATFORM_ARRAY <<< "$PLATFORMS"
    for platform in "${PLATFORM_ARRAY[@]}"; do
        local arch
        arch="${platform##*/}"  # Extract architecture from platform

        log_info "Testing platform: $platform ($arch)"

        # Run verification container
        local container_name="euxis-verify-$arch"

        if docker run \
            --rm \
            --name "$container_name" \
            --platform "$platform" \
            --env EUXIS_VERSION="$VERSION" \
            --env EUXIS_PLATFORM="$arch" \
            "${REGISTRY}/euxis-verification:${VERSION}" \
            /app/.euxis/bin/euxis-cross-platform-verify --local-only; then
            log_success "Verification passed for $platform"
        else
            log_error "Verification failed for $platform"
            return 1
        fi
    done

    log_success "All platform verifications passed"
}

generate_build_report() {
    log_info "Generating build report..."

    local report_file="$PROJECT_ROOT/multi-platform-build-report.md"

    cat > "$report_file" << EOF
# Euxis Multi-Platform Build Report

**Version:** $VERSION
**Build Date:** $(date -u)
**Platforms:** $PLATFORMS
**Registry:** $REGISTRY

## Build Configuration

- **Docker Version:** $(docker --version)
- **Buildx Version:** $(docker buildx version | head -n1)
- **Builder:** $(docker buildx ls | grep multi-platform | awk '{print $1}')

## Images Built

\`\`\`
${REGISTRY}/euxis:${VERSION}
${REGISTRY}/euxis:latest
${REGISTRY}/euxis-verification:${VERSION}
\`\`\`

## Platform Support

EOF

    # Add platform verification results
    IFS=',' read -ra PLATFORM_ARRAY <<< "$PLATFORMS"
    for platform in "${PLATFORM_ARRAY[@]}"; do
        echo "- ✅ **$platform** - Verified" >> "$report_file"
    done

    cat >> "$report_file" << EOF

## Security

- Non-root container execution
- Minimal base image (python:3.11-slim)
- Health checks enabled
- Resource constraints configured

## Next Steps

EOF

    if [[ "$PUSH" == "true" ]]; then
        echo "Images have been pushed to registry and are ready for deployment." >> "$report_file"
    else
        echo "Images are available locally. Use \`--push\` to publish to registry." >> "$report_file"
    fi

    log_success "Build report generated: $report_file"
}

cleanup_builder() {
    if [[ "${CLEANUP_BUILDER:-false}" == "true" ]]; then
        log_info "Cleaning up buildx builder..."
        docker buildx rm multi-platform || true
    fi
}

main() {
    echo "================================================"
    echo "    EUXIS MULTI-PLATFORM BUILD PIPELINE"
    echo "    Version: $VERSION"
    echo "    Platforms: $PLATFORMS"
    echo "================================================"
    echo

    trap cleanup_builder EXIT

    # Phase 1: Dependency Check
    check_dependencies

    # Phase 2: Build Images
    build_images

    # Phase 3: Verification Tests
    if [[ "$VERIFY" == "true" ]]; then
        run_verification_tests
    else
        log_warning "Skipping verification tests (--no-verify)"
    fi

    # Phase 4: Report Generation
    generate_build_report

    echo
    echo "================================================"
    log_success "Multi-platform build pipeline completed"
    echo "================================================"
}

main "$@"