#!/usr/bin/env python3
"""
Hyperparameter grid-tuner for ClusteredTriangulationAlgorithm.

This script:
 - edits the constants in src/core/ClusteredTriangulationAlgorithm.cpp
 - (optionally) builds the project with `make`
 - (optionally) runs an evaluation command you provide and parses a numeric metric
 - prints each tested setup and metric, then prints the best setup

Usage examples (from repo root):
  # dry-run (no build / eval) just list combos:
  ./scripts/tune_hyperparams.py --dry-run

  # run build and an evaluation command that prints "Total error: <value>"
  ./scripts/tune_hyperparams.py --eval-cmd "./build/tests/triangulation_tests --run-my-eval" --metric-regex "Total error:\\s*([0-9.+-eE]+)"

  # run build and evaluation on a specific file:
  ./scripts/tune_hyperparams.py --eval-cmd "./build/tests/triangulation_tests --run-my-eval" --input-file "recordings/new_field.json"

Notes:
 - The script will backup the original .cpp file and restore it at the end (or on Ctrl-C).
 - On macOS the script uses pure-Python file edits (no sed in-place).
 - Provide --eval-cmd to run a test/validation executable after each build and --metric-regex to extract a numeric metric from stdout.
"""

from __future__ import annotations
import argparse
import itertools
import os
import re
import shutil
import subprocess
import sys
import tempfile
from typing import Dict, Tuple, Optional, List

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPP_PATH = os.path.join(ROOT, "src", "core", "ClusteredTriangulationAlgorithm.cpp")
BACKUP_PATH = CPP_PATH + ".bak_tuning"

# regex patterns to locate assignments to the file-local constants
RE_PATTERNS = {
    "DEFAULT_COALITION_DISTANCE_METERS": re.compile(r'(DEFAULT_COALITION_DISTANCE_METERS\s*=\s*)([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?\d+)?)(\s*;)'),
    "CLUSTER_MIN_POINTS": re.compile(r'(CLUSTER_MIN_POINTS\s*=\s*)(\d+)(u?\s*;)'),
    "CLUSTER_RATIO_SPLIT_THRESHOLD": re.compile(r'(CLUSTER_RATIO_SPLIT_THRESHOLD\s*=\s*)([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?\d+)?)(\s*;)'),
    "GRADIENT_DESCENT_STEP_METERS": re.compile(r'(GRADIENT_DESCENT_STEP_METERS\s*=\s*)([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?\d+)?)(\s*;)'),
    "NORMAL_REGULARIZATION_EPS": re.compile(r'(NORMAL_REGULARIZATION_EPS\s*=\s*)([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?\d+)?)(\s*;)'),
    "GAUSS_ELIM_PIVOT_EPS": re.compile(r'(GAUSS_ELIM_PIVOT_EPS\s*=\s*)([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?\d+)?)(\s*;)'),
}


def read_file(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def write_file(path: str, contents: str) -> None:
    with open(path, "w", encoding="utf-8") as f:
        f.write(contents)


def replace_constants_in_cpp(replacements: Dict[str, str]) -> None:
    contents = read_file(CPP_PATH)
    for key, val in replacements.items():
        pat = RE_PATTERNS.get(key)
        if not pat:
            raise RuntimeError(f"No regex pattern for {key}")
        def _repl(m):
            return m.group(1) + val + m.group(3)
        new_contents, n = pat.subn(_repl, contents)
        if n == 0:
            raise RuntimeError(f"Failed to replace {key} in {CPP_PATH}")
        contents = new_contents
    write_file(CPP_PATH, contents)


def run_make() -> Tuple[int, str, str]:
    # run make (configure + build as Makefile defined) - capture output silently
    proc = subprocess.run(["make", "-C", ROOT], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return proc.returncode, proc.stdout, ""


def run_eval_cmd(cmd: str) -> Tuple[int, str]:
    proc = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return proc.returncode, proc.stdout


def extract_metric(output: str, metric_regex: Optional[str]) -> Optional[float]:
    if metric_regex:
        m = re.search(metric_regex, output)
        if m:
            try:
                return float(m.group(1))
            except Exception:
                return None
    # fallback common patterns (case-insensitive)
    fallbacks = [
        r'Global\s+Average\s+Error[:\s]+([0-9]+(?:\.[0-9]+)?)',
        r'Global\s+average\s+error[:\s]+([0-9]+(?:\.[0-9]+)?)',
        r'Average\s+Error[:\s]+([0-9]+(?:\.[0-9]+)?)',
    ]
    for patt in fallbacks:
        m = re.search(patt, output, flags=re.IGNORECASE)
        if m:
            try:
                return float(m.group(1))
            except Exception:
                return None
    return None


def grid_search(
    coalition_values: List[float],
    cluster_min_points_values: List[int],
    cluster_ratio_values: List[float],
    gradient_step_values: List[float],
    reg_eps_values: List[float],
    piv_eps_values: List[float],
    eval_cmd: Optional[str],
    metric_regex: Optional[str],
    dry_run: bool,
    max_tests: Optional[int],
    input_file: Optional[str],
):
    # backup original file
    if not os.path.exists(BACKUP_PATH):
        shutil.copy2(CPP_PATH, BACKUP_PATH)

    tried = 0
    results = []
    try:
        combos = itertools.product(
            coalition_values,
            cluster_min_points_values,
            cluster_ratio_values,
            gradient_step_values,
            reg_eps_values,
            piv_eps_values,
        )
        for combo in combos:
            if max_tests and tried >= max_tests:
                break
            coalition, min_pts, ratio, step, reg_eps, piv_eps = combo

            # ensure parameter limits sanity
            if not (0.0 < ratio <= 1.0):
                continue
            if min_pts < 1:
                continue

            replacements = {
                "DEFAULT_COALITION_DISTANCE_METERS": format_float_literal(coalition),
                "CLUSTER_MIN_POINTS": str(int(min_pts)),
                "CLUSTER_RATIO_SPLIT_THRESHOLD": format_float_literal(ratio),
                "GRADIENT_DESCENT_STEP_METERS": format_float_literal(step),
                "NORMAL_REGULARIZATION_EPS": format_float_literal(reg_eps),
                "GAUSS_ELIM_PIVOT_EPS": format_float_literal(piv_eps),
            }

            # concise single-line start message
            print(f"Test #{tried+1}: coalition={coalition}, min_pts={min_pts}, ratio={ratio}, step={step}, reg_eps={reg_eps}, piv_eps={piv_eps}", flush=True)

            metric = None
            if not dry_run:
                replace_constants_in_cpp(replacements)
                code, build_out, _ = run_make()
                if code != 0:
                    # only report failure concisely
                    print(f"  -> build FAILED for this configuration (skipping eval)", flush=True)
                    metric = None
                else:
                    if eval_cmd:
                        cmd_to_run = eval_cmd
                        if input_file:
                            cmd_to_run = f"{eval_cmd} {input_file}"
                        rc, out = run_eval_cmd(cmd_to_run)
                        metric = extract_metric(out, metric_regex)
                        # print only the extracted metric (or N/A)
                        if metric is not None:
                            print(f"  -> avg_error={metric}", flush=True)
                        else:
                            print(f"  -> avg_error=N/A (metric not found)", flush=True)
                    else:
                        print("  -> no eval-cmd provided", flush=True)
            else:
                print("  -> dry-run", flush=True)

            results.append((replacements.copy(), metric))
            tried += 1

    finally:
        # restore original file
        if os.path.exists(BACKUP_PATH):
            shutil.copy2(BACKUP_PATH, CPP_PATH)
            os.remove(BACKUP_PATH)

    # print concise per-test summary (one-line per test)
    print("\nPer-test results (concise):")
    for i, (repl, metric) in enumerate(results):
        params = f"coalition={repl['DEFAULT_COALITION_DISTANCE_METERS']},min_pts={repl['CLUSTER_MIN_POINTS']},ratio={repl['CLUSTER_RATIO_SPLIT_THRESHOLD']},step={repl['GRADIENT_DESCENT_STEP_METERS']}"
        print(f"#{i+1}: avg_error={metric if metric is not None else 'N/A'} -> {params}")

    # best
    valid_results = [(r, m) for (r, m) in results if m is not None]
    if valid_results:
        best = min(valid_results, key=lambda t: t[1])
        print("\nBest setup (lowest avg_error):")
        r, m = best
        print(f"avg_error={m}")
        print("params:", end=" ")
        print(f"coalition={r['DEFAULT_COALITION_DISTANCE_METERS']}, min_pts={r['CLUSTER_MIN_POINTS']}, ratio={r['CLUSTER_RATIO_SPLIT_THRESHOLD']}, step={r['GRADIENT_DESCENT_STEP_METERS']}")
    else:
        print("\nNo numeric metrics were collected (no eval-cmd or metric not found).")
        print("You can run again with --eval-cmd and --metric-regex to collect a numeric metric per test.")


def format_float_literal(v: float) -> str:
    # if very small/large, use scientific notation
    if abs(v) != 0.0 and (abs(v) < 1e-3 or abs(v) >= 1e6):
        return f"{v:.6e}"
    # keep decimals for typical floats
    return repr(float(v))


def parse_list(s: str, cast):
    if s.strip() == "":
        return []
    return [cast(x) for x in s.split(",")]


def main():
    p = argparse.ArgumentParser(description="Tune ClusteredTriangulationAlgorithm file-local hyperparameters.")
    p.add_argument("--coalition", default="3.0,2.0,5.0", help="comma-separated coalition distances (meters)")
    p.add_argument("--min-pts", default="7,5,9", help="comma-separated cluster min points (ints)")
    p.add_argument("--ratio", default="0.2,0.35,0.6", help="comma-separated cluster ratio values (0..1)")
    p.add_argument("--step", default="0.1,0.2", help="comma-separated gradient step sizes (meters)")
    p.add_argument("--reg-eps", default="1e-12,1e-9", help="comma-separated normal regularization epsilons")
    p.add_argument("--piv-eps", default="1e-15,1e-12", help="comma-separated gaussian pivot epsilons")
    p.add_argument("--eval-cmd", default=None, help="command to run after successful build that prints a numeric metric")
    p.add_argument("--input-file", type=str, help="Path to a single file to be processed by the eval command.")
    p.add_argument("--metric-regex", default=None, help="regex with one capture group to extract numeric metric from eval stdout")
    p.add_argument("--dry-run", action="store_true", help="do not modify files or build; only print combos")
    p.add_argument("--max-tests", type=int, default=0, help="limit number of tests (0 = unlimited)")

    args = p.parse_args()

    coalition_values = parse_list(args.coalition, float)
    cluster_min_points_values = parse_list(args.min_pts, int)
    cluster_ratio_values = parse_list(args.ratio, float)
    gradient_step_values = parse_list(args.step, float)
    reg_eps_values = parse_list(args.reg_eps, float)
    piv_eps_values = parse_list(args.piv_eps, float)

    max_tests = args.max_tests if args.max_tests > 0 else None

    grid_search(
        coalition_values,
        cluster_min_points_values,
        cluster_ratio_values,
        gradient_step_values,
        reg_eps_values,
        piv_eps_values,
        args.eval_cmd,
        args.metric_regex,
        args.dry_run,
        max_tests,
        args.input_file,
    )


if __name__ == "__main__":
    main()