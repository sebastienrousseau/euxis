# SPDX-License-Identifier: AGPL-3.0-or-later
"""Unit tests for the complexity ratchet (check_complexity.py).

The comparison logic is tested directly; the end-to-end tests run the
script against a throwaway git repository and need lizard on PATH.
"""
from __future__ import annotations

import importlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))

cc = importlib.import_module("check_complexity")

HOT = {"ccn": 12, "cognitive": 9, "halstead_difficulty": 20.0, "length": 40}


def csv_row(sig: str, ccn: int, length: int, cog: int, hdiff: float) -> str:
    """One lizard CSV row for a function in a.cpp."""
    name = sig.split("(", maxsplit=1)[0]
    return (f'1,{ccn},10,0,{length},"{name}@1-{length}@a.cpp","a.cpp",'
            f'"{name}","{sig}",1,{length},{cog},100.0,{hdiff},1000.0')


class ParseTest(unittest.TestCase):
    def test_reads_the_four_metrics(self):
        got = cc.parse_lizard_csv(csv_row("f(int x)", 12, 70, 16, 31.456))
        self.assertEqual(got, {"a.cpp::f(int x)": {
            "ccn": 12, "cognitive": 16, "halstead_difficulty": 31.46, "length": 70}})

    def test_signature_clash_keeps_the_worst_of_each(self):
        text = "\n".join([csv_row("f()", 12, 5, 1, 1.0),
                          csv_row("f()", 3, 80, 2, 2.0)])
        got = cc.parse_lizard_csv(text)["a.cpp::f()"]
        self.assertEqual((got["ccn"], got["length"]), (12, 80))


class CompareFunctionsTest(unittest.TestCase):
    def test_new_offender_is_an_error(self):
        errors, stale = cc.compare_functions({"f": HOT}, {})
        self.assertEqual(len(errors), 1)
        self.assertIn("new offender", errors[0])
        self.assertEqual(stale, [])

    def test_worse_on_an_over_ceiling_metric_is_an_error(self):
        errors, _ = cc.compare_functions({"f": {**HOT, "ccn": 13}}, {"f": HOT})
        self.assertIn("ccn 12 -> 13", errors[0])

    def test_crossing_a_second_ceiling_is_an_error(self):
        errors, _ = cc.compare_functions({"f": {**HOT, "cognitive": 16}}, {"f": HOT})
        self.assertIn("cognitive 9 -> 16", errors[0])

    def test_change_under_the_ceilings_only_needs_an_update(self):
        current = {"f": {**HOT, "cognitive": 14}}
        errors, stale = cc.compare_functions(current, {"f": HOT})
        self.assertEqual(errors, [])
        self.assertIn("changed", stale[0])

    def test_fixed_function_leaves_a_stale_entry(self):
        errors, stale = cc.compare_functions({}, {"f": HOT})
        self.assertEqual(errors, [])
        self.assertIn("fixed or gone: f", stale[0])

    def test_unchanged_offender_passes(self):
        self.assertEqual(cc.compare_functions({"f": HOT}, {"f": HOT}), ([], []))


class CompareFilesTest(unittest.TestCase):
    def test_growth_new_and_shrink(self):
        errors, stale = cc.compare_files({"a": 600, "b": 700, "c": 550},
                                         {"a": 600, "b": 650, "c": 560, "d": 900})
        self.assertEqual(len(errors), 1)
        self.assertIn("file grew: b", errors[0])
        self.assertEqual(len(stale), 2)


class ReportTest(unittest.TestCase):
    def test_stale_fails_without_update_and_passes_with_it(self):
        self.assertEqual(cc.report([], ["x"], update=False), 1)
        self.assertEqual(cc.report([], ["x"], update=True), 0)

    def test_errors_fail_even_with_update(self):
        self.assertEqual(cc.report(["x"], [], update=True), 1)


def complex_function(name: str, branches: int) -> str:
    body = "".join(f"    if (x == {i}) return {i};\n" for i in range(branches))
    return f"int {name}(int x) {{\n{body}    return -1;\n}}\n"


@unittest.skipUnless(shutil.which("lizard"), "lizard not installed")
class EndToEndTest(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.root = Path(self._td.name)
        self.baseline = self.root / "baseline.json"
        subprocess.run(["git", "init", "-q"], cwd=self.root, check=True)
        (self.root / "libs").mkdir()
        self.write("libs/a.cpp", complex_function("old_hot", 12))

    def tearDown(self):
        self._td.cleanup()

    def write(self, rel: str, text: str) -> None:
        (self.root / rel).write_text(text, encoding="utf-8")
        subprocess.run(["git", "add", rel], cwd=self.root, check=True)

    def run_gate(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPTS_DIR / "check_complexity.py"),
             "--repo-root", str(self.root), "--baseline", str(self.baseline), *args],
            capture_output=True, text=True, check=False)

    def test_ratchet(self):
        self.assertEqual(self.run_gate("--update").returncode, 0)
        recorded = json.loads(self.baseline.read_text())["functions"]
        self.assertEqual(list(recorded), ["libs/a.cpp::old_hot( int x)"])
        self.assertEqual(self.run_gate().returncode, 0)

        self.write("libs/b.cpp", complex_function("new_hot", 12))
        result = self.run_gate()
        self.assertEqual(result.returncode, 1)
        self.assertIn("new offender: libs/b.cpp::new_hot", result.stderr)
        self.assertEqual(self.run_gate("--update").returncode, 1)

        self.write("libs/b.cpp", complex_function("new_hot", 3))
        self.write("libs/a.cpp", complex_function("old_hot", 3))
        result = self.run_gate()
        self.assertEqual(result.returncode, 1)
        self.assertIn("fixed or gone: libs/a.cpp::old_hot", result.stderr)
        self.assertEqual(self.run_gate("--update").returncode, 0)
        self.assertEqual(json.loads(self.baseline.read_text())["functions"], {})


if __name__ == "__main__":
    unittest.main()
