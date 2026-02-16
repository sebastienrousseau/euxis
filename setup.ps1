#Requires -Version 5.1
<#
.SYNOPSIS
    Euxis Fleet Installer and Build Helper for Windows PowerShell.

.DESCRIPTION
    PowerShell equivalent of setup.sh and the Makefile. By default (no arguments)
    it runs the installer: creates ~/bin, symlinks tools from ~/.euxis/cli/bin, runs
    the health check, and ensures ~/bin is on PATH.

    Pass -Target to run Makefile-equivalent build commands:
      .\setup.ps1 -Target install
      .\setup.ps1 -Target test
      .\setup.ps1 -Target ci-check

.NOTES
    Run from an elevated (Administrator) prompt if you want true symlinks
    on Windows without Developer Mode enabled.
#>

param(
    [ValidateSet(
        'setup',
        'install',
        'install-dev',
        'clean',
        'lint',
        'format',
        'format-fix',
        'type-check',
        'security',
        'test',
        'test-cov',
        'test-all',
        'ci-check',
        'ci-local',
        'dev-setup',
        'quick-check',
        'version',
        'info',
        'show-coverage',
        'help'
    )]
    [string]$Target = 'setup'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Python = 'python'

# =====================================================================
#  Installer  (setup.sh equivalent)
# =====================================================================

function Invoke-EuxisSetup {
    $EuxisHome   = Join-Path $env:USERPROFILE '.euxis'
    $EuxisBinDir = Join-Path $EuxisHome 'bin'
    $UserBinDir  = Join-Path $env:USERPROFILE 'bin'

    # ── 1. Create ~/bin ──────────────────────────────────────────────
    Write-Host 'Installing Euxis Fleet...'

    if (-not (Test-Path $UserBinDir)) {
        New-Item -ItemType Directory -Path $UserBinDir -Force | Out-Null
        Write-Host "  Created $UserBinDir"
    }

    # ── 2. Link / copy tools to ~/bin ────────────────────────────────
    Write-Host 'Linking tools to ~/bin...'

    if (Test-Path $EuxisBinDir) {
        Get-ChildItem -Path $EuxisBinDir -File | ForEach-Object {
            $script = $_
            $name   = $script.BaseName

            # Skip .sh wrapper if a wrapper-less executable with the same
            # base name already exists (mirrors the bash logic)
            if ($script.Extension -eq '.sh') {
                $plain = Join-Path $EuxisBinDir $name
                if (Test-Path $plain) { return }   # continue in ForEach-Object
            }

            $targetPath = Join-Path $UserBinDir $name

            # Prefer symlink; fall back to copy when symlinks are unavailable
            try {
                if (Test-Path $targetPath) { Remove-Item $targetPath -Force }
                New-Item -ItemType SymbolicLink -Path $targetPath -Value $script.FullName -Force | Out-Null
            }
            catch {
                Copy-Item -Path $script.FullName -Destination $targetPath -Force
            }
            Write-Host "  - $name"
        }
    }
    else {
        Write-Warning "$EuxisBinDir not found. Skipping tool linking."
    }

    # ── 3. Health check ──────────────────────────────────────────────
    Write-Host 'Verifying protocol headers...'

    $healthCmd = Join-Path $UserBinDir 'euxis-health'
    if (Test-Path $healthCmd) {
        try {
            & $healthCmd --silent
        }
        catch {
            Write-Host "Health check flagged issues. Run 'euxis-health' for details."
        }
    }
    else {
        Write-Host '  euxis-health not found; skipping health check.'
    }

    # ── 4. Ensure ~/bin is on PATH (session + persistent) ────────────
    $currentPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($currentPath -notlike "*$UserBinDir*") {
        [Environment]::SetEnvironmentVariable('Path', "$UserBinDir;$currentPath", 'User')
        $env:Path = "$UserBinDir;$env:Path"
        Write-Host "  Added $UserBinDir to user PATH."
    }

    # ── 5. Done ──────────────────────────────────────────────────────
    Write-Host ''
    Write-Host 'Euxis is ready.'
    Write-Host '   Try:  euxis butler "Introduce yourself briefly."'
    Write-Host '   Or:   euxis verify'
    Write-Host ''
    Write-Host 'Restart your terminal to refresh PATH.'
}

# =====================================================================
#  Build helpers  (Makefile equivalents)
# =====================================================================

function Invoke-Target {
    param([string]$Name)

    switch ($Name) {

        'setup' {
            Invoke-EuxisSetup
        }

        'help' {
            Write-Host 'Euxis Project - Build Targets (PowerShell)' -ForegroundColor Blue
            Write-Host ''
            Write-Host 'Available targets:' -ForegroundColor Yellow
            $targets = @(
                @('setup',        'Run the Euxis installer (default)'),
                @('install',      'Install production dependencies'),
                @('install-dev',  'Install development dependencies'),
                @('clean',        'Clean build artifacts and cache'),
                @('lint',         'Run all linters (zero-warning policy)'),
                @('format',       'Check code formatting'),
                @('format-fix',   'Fix code formatting'),
                @('type-check',   'Run type checker in strict mode'),
                @('security',     'Run security scans'),
                @('test',         'Run test suite'),
                @('test-cov',     'Run tests with 100% coverage requirement'),
                @('test-all',     'Run all tests including integration'),
                @('ci-check',     'Simulate CI pipeline locally'),
                @('ci-local',     'Install dev deps + full CI simulation'),
                @('dev-setup',    'Set up development environment'),
                @('quick-check',  'Quick lint + format + test'),
                @('version',      'Show current version'),
                @('info',         'Show project info'),
                @('show-coverage','Open coverage report in browser')
            )
            foreach ($t in $targets) {
                Write-Host ("  {0,-16} {1}" -f $t[0], $t[1]) -ForegroundColor Green
            }
        }

        'install' {
            Write-Host 'Installing production dependencies...' -ForegroundColor Blue
            & $Python -m pip install --upgrade pip
            & $Python -m pip install -r requirements.txt
        }

        'install-dev' {
            Write-Host 'Installing development dependencies...' -ForegroundColor Blue
            & $Python -m pip install --upgrade pip
            & $Python -m pip install -r requirements.txt
            & $Python -m pip install -e '.[dev]'
        }

        'clean' {
            Write-Host 'Cleaning up...' -ForegroundColor Blue
            $dirsToRemove = @(
                'build', 'dist', 'htmlcov',
                '.pytest_cache', '.mypy_cache', '.ruff_cache'
            )
            foreach ($d in $dirsToRemove) {
                if (Test-Path $d) { Remove-Item $d -Recurse -Force }
            }
            $filesToRemove = @('.coverage')
            foreach ($f in $filesToRemove) {
                if (Test-Path $f) { Remove-Item $f -Force }
            }
            # Remove __pycache__ and *.pyc recursively
            Get-ChildItem -Recurse -Directory -Filter '__pycache__' -ErrorAction SilentlyContinue |
                Remove-Item -Recurse -Force
            Get-ChildItem -Recurse -Filter '*.pyc' -ErrorAction SilentlyContinue |
                Remove-Item -Force
            Get-ChildItem -Recurse -Filter '*.egg-info' -Directory -ErrorAction SilentlyContinue |
                Remove-Item -Recurse -Force
            Get-ChildItem -Filter 'coverage*.xml' -ErrorAction SilentlyContinue |
                Remove-Item -Force
            Get-ChildItem -Filter '*-report.json' -ErrorAction SilentlyContinue |
                Remove-Item -Force
            Get-ChildItem -Filter '*-report.md' -ErrorAction SilentlyContinue |
                Remove-Item -Force
            Write-Host 'Clean complete.' -ForegroundColor Green
        }

        'lint' {
            Write-Host 'Running linters with zero-warning policy...' -ForegroundColor Blue
            Write-Host 'Ruff linter...' -ForegroundColor Yellow
            & ruff check .
            if ($LASTEXITCODE -ne 0) { throw 'ruff check failed' }
            Write-Host 'Flake8 linter...' -ForegroundColor Yellow
            & flake8 --statistics --count --max-line-length=100 --extend-ignore=E203,W503 .
            if ($LASTEXITCODE -ne 0) { throw 'flake8 failed' }
            Write-Host 'All linting checks passed' -ForegroundColor Green
        }

        'format' {
            Write-Host 'Checking code formatting...' -ForegroundColor Blue
            Write-Host 'Black formatter check...' -ForegroundColor Yellow
            & black --check --diff --color .
            if ($LASTEXITCODE -ne 0) { throw 'black check failed' }
            Write-Host 'isort import sorting check...' -ForegroundColor Yellow
            & isort --check-only --diff --color .
            if ($LASTEXITCODE -ne 0) { throw 'isort check failed' }
            Write-Host 'Code formatting is correct' -ForegroundColor Green
        }

        'format-fix' {
            Write-Host 'Fixing code formatting...' -ForegroundColor Blue
            & black .
            & isort .
            Write-Host 'Code formatting fixed' -ForegroundColor Green
        }

        'type-check' {
            Write-Host 'Running type checker...' -ForegroundColor Blue
            & mypy --ignore-missing-imports --disallow-untyped-defs tui/
            if ($LASTEXITCODE -ne 0) { throw 'mypy failed' }
            Write-Host 'Type checking passed' -ForegroundColor Green
        }

        'security' {
            Write-Host 'Running security scans...' -ForegroundColor Blue
            Write-Host 'Dependency vulnerability scan (pip-audit)...' -ForegroundColor Yellow
            & pip-audit
            if ($LASTEXITCODE -ne 0) { throw 'pip-audit failed' }
            Write-Host 'Alternative dependency scan (safety)...' -ForegroundColor Yellow
            & safety check
            if ($LASTEXITCODE -ne 0) { throw 'safety check failed' }
            Write-Host 'Python security scan (bandit)...' -ForegroundColor Yellow
            & bandit -r . -ll -x ./tests/,./venv/,./env/
            if ($LASTEXITCODE -ne 0) { throw 'bandit failed' }
            Write-Host 'All security scans passed' -ForegroundColor Green
        }

        'test' {
            Write-Host 'Running test suite...' -ForegroundColor Blue
            & $Python -m pytest tests/ -v --tb=short
            if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
            Write-Host 'Tests passed' -ForegroundColor Green
        }

        'test-cov' {
            Write-Host 'Running test suite with coverage (100% required)...' -ForegroundColor Blue
            & $Python -m pytest tests/ -v `
                --cov=tui `
                --cov-report=term-missing `
                --cov-report=html:htmlcov `
                --cov-report=xml:coverage.xml `
                --cov-fail-under=100 `
                --maxfail=5 `
                --tb=short
            if ($LASTEXITCODE -ne 0) { throw 'test-cov failed' }
            Write-Host 'Tests passed with 100% coverage' -ForegroundColor Green
        }

        'test-all' {
            Write-Host 'Running all tests including integration...' -ForegroundColor Blue
            & $Python -m pytest tests/ -v --maxfail=10 --tb=long
            if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
            Write-Host 'All tests completed' -ForegroundColor Green
        }

        'ci-check' {
            Write-Host 'Running CI pipeline simulation...' -ForegroundColor Blue
            Write-Host ''
            Write-Host '===== 1. LINT & FORMAT CHECK =====' -ForegroundColor Blue
            Invoke-Target 'lint'
            Invoke-Target 'format'
            Write-Host ''
            Write-Host '===== 2. TYPE CHECK =====' -ForegroundColor Blue
            Invoke-Target 'type-check'
            Write-Host ''
            Write-Host '===== 3. SECURITY SCAN =====' -ForegroundColor Blue
            Invoke-Target 'security'
            Write-Host ''
            Write-Host '===== 4. TEST WITH COVERAGE =====' -ForegroundColor Blue
            Invoke-Target 'test-cov'
            Write-Host ''
            Write-Host 'ALL CI CHECKS PASSED! Ready for merge.' -ForegroundColor Green
        }

        'ci-local' {
            Invoke-Target 'install-dev'
            Invoke-Target 'ci-check'
        }

        'dev-setup' {
            Invoke-Target 'install-dev'
            Write-Host 'Setting up development environment...' -ForegroundColor Blue
            if (Get-Command pre-commit -ErrorAction SilentlyContinue) {
                & pre-commit install
            }
            Write-Host 'Development environment ready' -ForegroundColor Green
        }

        'quick-check' {
            Write-Host 'Running quick development checks...' -ForegroundColor Blue
            Invoke-Target 'lint'
            Invoke-Target 'format'
            Invoke-Target 'test'
            Write-Host 'Quick checks passed' -ForegroundColor Green
        }

        'version' {
            $ver = Select-String -Path pyproject.toml -Pattern 'version\s*=\s*"([^"]+)"' |
                   Select-Object -First 1
            if ($ver) {
                Write-Host "Current version: $($ver.Matches[0].Groups[1].Value)" -ForegroundColor Blue
            }
        }

        'info' {
            Write-Host 'Euxis Project Information:' -ForegroundColor Blue
            $pyVer = & $Python --version 2>&1
            Write-Host "Python version: $pyVer"
            $ver = Select-String -Path pyproject.toml -Pattern 'version\s*=\s*"([^"]+)"' |
                   Select-Object -First 1
            if ($ver) {
                Write-Host "Project version: $($ver.Matches[0].Groups[1].Value)"
            }
            $prefix = & $Python -c "import sys; print(sys.prefix)" 2>&1
            Write-Host "Install location: $prefix"
            $pkgCount = (& $Python -m pip list 2>$null | Measure-Object -Line).Lines
            Write-Host "Dependencies installed: $pkgCount packages"
        }

        'show-coverage' {
            $htmlCov = Join-Path (Get-Location) 'htmlcov' 'index.html'
            if (Test-Path $htmlCov) {
                Write-Host 'Opening coverage report...' -ForegroundColor Blue
                Start-Process $htmlCov
            }
            else {
                Write-Host "No coverage report found. Run '.\setup.ps1 -Target test-cov' first." -ForegroundColor Red
            }
        }
    }
}

# ── Entry point ──────────────────────────────────────────────────────
Invoke-Target $Target
