# Euxis Zero-Tolerance Development Makefile
# Enforces strict code quality standards

.PHONY: help install install-dev clean lint format type-check test test-coverage security audit ci-local

# Default Python interpreter
PYTHON := python3
PIP := $(PYTHON) -m pip

# Directories
SRC_DIR := tui
TEST_DIR := tests
COVERAGE_DIR := $(TEST_DIR)/coverage

help: ## Show this help message
	@echo "Euxis Development Commands (Zero-Tolerance Policy)"
	@echo "=================================================="
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

install: ## Install production dependencies
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements.txt

install-dev: ## Install development dependencies
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements-dev.txt

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
	ruff check . --fix --show-fixes
	@echo "✅ Linting complete"

lint-check: ## Check linting (ZERO TOLERANCE)
	@echo "=== Checking linting violations ==="
	ruff check .
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