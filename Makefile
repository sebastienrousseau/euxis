# Euxis Zero-Tolerance Development Makefile with UV
# Enforces strict code quality standards with modern dependency management

.PHONY: help install install-dev install-api install-db clean lint format type-check test test-coverage security audit ci-local setup-env lock sync update

# Default Python interpreter and package manager
PYTHON := $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null)
UV := $(shell command -v uv 2>/dev/null)
PIP := $(PYTHON) -m pip

# Project directories
PROJECT_DIR := $(shell pwd)
VENV_DIR := $(PROJECT_DIR)/.venv

# Directories
SRC_DIR := tui
TEST_DIR := tests
COVERAGE_DIR := $(TEST_DIR)/coverage
RUFF_CACHE_DIR ?= /tmp/euxis-ruff-cache

help: ## Show this help message
	@echo "Euxis Development Commands (Zero-Tolerance Policy)"
	@echo "=================================================="
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

check-uv: ## Check if UV is installed
	@if [ -z "$(UV)" ]; then \
		echo "❌ UV is not installed"; \
		echo "Install UV with: curl -LsSf https://astral.sh/uv/install.sh | sh"; \
		exit 1; \
	fi

setup-env: ## Complete environment setup with UV
	@echo "=== Setting up UV-based environment ==="
	@chmod +x scripts/setup-env.sh
	@scripts/setup-env.sh
	@echo "✅ Environment setup complete"

install: check-uv ## Install production dependencies with UV
	@echo "=== Installing production dependencies ==="
	$(UV) pip install -e .
	@echo "✅ Production dependencies installed"

install-dev: check-uv ## Install development dependencies with UV
	@echo "=== Installing development dependencies ==="
	$(UV) pip install -e ".[dev]"
	@echo "✅ Development dependencies installed"

install-api: check-uv ## Install API dependencies with UV
	@echo "=== Installing API dependencies ==="
	$(UV) pip install -e ".[api]"
	@echo "✅ API dependencies installed"

install-db: check-uv ## Install database dependencies with UV
	@echo "=== Installing database dependencies ==="
	$(UV) pip install -e ".[db]"
	@echo "✅ Database dependencies installed"

install-all: check-uv ## Install all dependencies with UV
	@echo "=== Installing all dependencies ==="
	$(UV) pip install -e ".[dev,api,db]"
	@echo "✅ All dependencies installed"

lock: check-uv ## Generate UV lock files
	@echo "=== Generating UV lock files ==="
	$(UV) pip compile pyproject.toml --output-file requirements.lock
	$(UV) pip compile pyproject.toml --extra dev --output-file requirements-dev.lock
	$(UV) pip compile pyproject.toml --extra api --output-file requirements-api.lock
	$(UV) pip compile pyproject.toml --extra db --output-file requirements-db.lock
	@echo "✅ Lock files generated"

sync: check-uv ## Sync dependencies from lock files
	@echo "=== Syncing dependencies from lock files ==="
	@if [ -f "requirements.lock" ]; then \
		$(UV) pip sync requirements.lock; \
		echo "✅ Dependencies synced"; \
	else \
		echo "⚠️  No lock file found, run 'make lock' first"; \
	fi

update: check-uv ## Update all dependencies
	@echo "=== Updating all dependencies ==="
	$(UV) pip install --upgrade -e ".[dev,api,db]"
	@echo "✅ Dependencies updated"

clean: ## Clean up generated files
	find . -type f -name "*.pyc" -delete
	find . -type d -name "__pycache__" -delete
	find . -type d -name "*.egg-info" -exec rm -rf {} +
	rm -rf build/ dist/ .coverage $(COVERAGE_DIR)/ .pytest_cache/
	find . -name "*.orig" -delete

format: ## Format code with Black and isort (REQUIRED)
	@echo "=== Formatting code (Black + isort) ==="
	black .
	isort .
	@echo "✅ Code formatting complete"

format-check: ## Check code formatting (ZERO TOLERANCE)
	@echo "=== Checking code formatting ==="
	black --check --diff .
	isort --check-only --diff .
	@echo "✅ Code formatting verified"

lint: ## Lint code with Ruff (ZERO WARNINGS)
	@echo "=== Linting code (Ruff - ZERO WARNINGS) ==="
	@mkdir -p $(RUFF_CACHE_DIR)
	RUFF_CACHE_DIR=$(RUFF_CACHE_DIR) ruff check . --fix --show-fixes
	@echo "✅ Linting complete"

lint-check: ## Check linting (ZERO TOLERANCE)
	@echo "=== Checking linting violations ==="
	@mkdir -p $(RUFF_CACHE_DIR)
	RUFF_CACHE_DIR=$(RUFF_CACHE_DIR) ruff check .
	@echo "✅ No linting violations found"

type-check: ## Type check with mypy (STRICT MODE)
	@echo "=== Type checking (mypy - STRICT MODE) ==="
	mypy $(SRC_DIR)/ --strict
	@echo "✅ Type checking passed"

test: ## Run test suite
	@echo "=== Running test suite ==="
	pytest -v
	@echo "✅ Tests completed"

test-coverage: ## Run tests with 100% coverage requirement
	@echo "=== Running tests with 100% coverage requirement ==="
	pytest --cov=$(SRC_DIR) --cov=bin --cov-report=term-missing --cov-report=html --cov-fail-under=100
	@echo "✅ 100% test coverage achieved"

security: ## Run security scans (ZERO TOLERANCE)
	@echo "=== Running security scans ==="
	bandit -r . -x ./tests/,./venv/,./env/,./docs_backup_*
	pip-audit
	@echo "✅ Security scans passed"

audit: ## Full dependency audit
	@echo "=== Running full dependency audit ==="
	pip-audit --format=table
	safety check
	@echo "✅ Dependency audit complete"

ci-local: clean format-check lint-check type-check test-coverage security ## Run full CI pipeline locally
	@echo "======================================="
	@echo "✅ ALL CI CHECKS PASSED"
	@echo "======================================="
	@echo "Code is ready for submission"

ci-fix: clean format lint test ## Auto-fix formatting and linting issues
	@echo "======================================="
	@echo "✅ AUTO-FIX COMPLETE"
	@echo "======================================="
	@echo "Run 'make ci-local' to verify all checks pass"

# Development workflow targets
dev-setup: install-dev ## Set up development environment
	@echo "=== Development environment setup complete ==="

pre-commit: format lint type-check test ## Pre-commit hook (auto-fix)
	@echo "✅ Pre-commit checks complete"

pre-push: ci-local ## Pre-push hook (strict validation)
	@echo "✅ Pre-push validation complete"
