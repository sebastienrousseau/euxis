.PHONY: all test lint format clean install dev

all: install test

test:
	pytest

test-cov:
	pytest --cov=euxis-core/src --cov-report=term-missing

lint:
	ruff check .

format:
	ruff format .

clean:
	find . -type d -name "__pycache__" -exec rm -rf {} +
	find . -type d -name ".pytest_cache" -exec rm -rf {} +
	find . -type d -name ".ruff_cache" -exec rm -rf {} +

install:
	pip install -e ".[dev]"
