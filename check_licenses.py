#!/usr/bin/env python3
import requests
import json
import time
from typing import Dict, List, Optional, Set
import sys

# Copyleft licenses that may be problematic in permissive projects
COPYLEFT_LICENSES = {
    'GPL', 'GPL-2.0', 'GPL-3.0', 'AGPL', 'AGPL-3.0', 'LGPL', 'LGPL-2.1', 'LGPL-3.0',
    'GNU GPL', 'GNU GENERAL PUBLIC LICENSE', 'GNU AFFERO GENERAL PUBLIC LICENSE',
    'GNU Lesser General Public License', 'GNU LGPL'
}

# License normalization mapping
LICENSE_ALIASES = {
    'BSD': 'BSD License',
    'MIT': 'MIT License',
    'Apache': 'Apache License',
    'Apache 2.0': 'Apache Software License',
    'Apache-2.0': 'Apache Software License',
    'ISC': 'ISC License',
    'MPL': 'Mozilla Public License'
}

def get_package_license(package_name: str, version: str) -> Optional[Dict]:
    """Get package license info from PyPI API"""
    try:
        clean_name = package_name.strip()
        url = f"https://pypi.org/pypi/{clean_name}/json"
        response = requests.get(url, timeout=10)
        if response.status_code == 200:
            data = response.json()
            info = data.get('info', {})

            license_info = {
                'name': clean_name,
                'version': version,
                'license': info.get('license', ''),
                'license_expression': info.get('license_expression', ''),
                'classifiers': [c for c in info.get('classifiers', []) if 'License' in c],
                'author': info.get('author', ''),
                'home_page': info.get('home_page', ''),
                'project_url': info.get('project_url', '')
            }
            return license_info
        else:
            print(f"Warning: Could not fetch license info for {clean_name}: HTTP {response.status_code}", file=sys.stderr)
            return None
    except Exception as e:
        print(f"Error fetching license for {package_name}: {e}", file=sys.stderr)
        return None

def classify_license(license_info: Dict) -> str:
    """Classify license as permissive, copyleft, or unknown"""
    all_text = f"{license_info['license']} {license_info['license_expression']} {' '.join(license_info['classifiers'])}"
    all_text_upper = all_text.upper()

    # Check for copyleft licenses
    for copyleft in COPYLEFT_LICENSES:
        if copyleft.upper() in all_text_upper:
            return 'COPYLEFT'

    # Check for common permissive licenses
    permissive_indicators = ['MIT', 'BSD', 'APACHE', 'ISC', 'PYTHON SOFTWARE FOUNDATION', 'UNLICENSE', 'PUBLIC DOMAIN']
    for permissive in permissive_indicators:
        if permissive in all_text_upper:
            return 'PERMISSIVE'

    # If we can't determine, mark as unknown
    if not all_text.strip():
        return 'UNKNOWN'

    return 'OTHER'

def parse_requirements_file(file_path: str) -> List[tuple]:
    """Parse requirements-lock.txt and extract package names and versions"""
    packages = []
    try:
        with open(file_path, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    if '==' in line:
                        name, version = line.split('==', 1)
                        packages.append((name.strip(), version.strip()))
    except Exception as e:
        print(f"Error reading requirements file: {e}", file=sys.stderr)
    return packages

def check_licenses():
    """Check licenses of all packages"""
    packages = parse_requirements_file('requirements-lock.txt')
    print(f"Checking licenses for {len(packages)} packages...")

    results = {
        'permissive': [],
        'copyleft': [],
        'other': [],
        'unknown': [],
        'errors': []
    }

    for i, (name, version) in enumerate(packages):
        if i > 0:
            time.sleep(0.5)  # Rate limit PyPI API calls
        print(f"Checking {i+1}/{len(packages)}: {name}...", end=' ')

        license_info = get_package_license(name, version)
        if not license_info:
            results['errors'].append({'name': name, 'version': version})
            print("ERROR")
            continue

        classification = classify_license(license_info)
        license_info['classification'] = classification

        if classification == 'COPYLEFT':
            results['copyleft'].append(license_info)
            license_text = license_info['license'] or ""
            print(f"COPYLEFT ({license_text[:50]}...)")
        elif classification == 'PERMISSIVE':
            results['permissive'].append(license_info)
            license_text = license_info['license'] or ""
            print(f"PERMISSIVE ({license_text[:30]}...)")
        elif classification == 'UNKNOWN':
            results['unknown'].append(license_info)
            print("UNKNOWN")
        else:
            results['other'].append(license_info)
            license_text = license_info['license'] or ""
            print(f"OTHER ({license_text[:30]}...)")

    return results

if __name__ == "__main__":
    results = check_licenses()

    print(f"\n=== LICENSE CLASSIFICATION SUMMARY ===")
    print(f"Permissive licenses: {len(results['permissive'])}")
    print(f"Copyleft licenses (potential issue): {len(results['copyleft'])}")
    print(f"Other licenses: {len(results['other'])}")
    print(f"Unknown licenses: {len(results['unknown'])}")
    print(f"Errors: {len(results['errors'])}")

    if results['copyleft']:
        print(f"\n=== COPYLEFT LICENSES (POTENTIAL ISSUE) ===")
        for pkg in results['copyleft']:
            print(f"{pkg['name']}=={pkg['version']}")
            print(f"  License: {pkg['license']}")
            print(f"  Classifiers: {pkg['classifiers']}")
            print()

    if results['unknown']:
        print(f"\n=== UNKNOWN LICENSES (NEED MANUAL REVIEW) ===")
        for pkg in results['unknown']:
            print(f"{pkg['name']}=={pkg['version']}")
            print(f"  License field: '{pkg['license']}'")
            print(f"  Classifiers: {pkg['classifiers']}")
            print()

    # Save full results to JSON
    with open('license_report.json', 'w') as f:
        json.dump(results, f, indent=2, default=str)

    print(f"Full license report saved to license_report.json")