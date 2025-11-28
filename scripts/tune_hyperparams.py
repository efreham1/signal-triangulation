#!/usr/bin/env python3
"""
Hyperparameter command-line tuner for the triangulation algorithm.

This script constructs and executes command-line calls to the main executable,
passing hyperparameter values as named arguments (e.g., --cta-coalition 3.0).

Usage Example (from repo root):
  ./scripts/tune_hyperparams.py \
    --eval-cmd "./build/tests/triangulation_tests --gtest_filter=Triangulation.SingleFileErrorCheck --run-single-file recordings/new_field.json --algorithm CTA2" \
    --metric-regex "Global Average Error:\s*([0-9.+-eE]+)" \
    --min-pts "5,7,9,11" \
    --ratio "0.2,0.35,0.5" \
    --coalition "2.0,3.0,4.0" \
    --max-tests 100
"""

from __future__ import annotations
import argparse
import itertools
import subprocess
import re
import sys
from typing import Dict, Optional, Tuple

def run_eval_cmd(cmd: str) -> Tuple[int, str]:
    """Executes the provided shell command and captures its output."""
    proc = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
    return proc.returncode, proc.stdout

def extract_metric(output: str, metric_regex: Optional[str]) -> Optional[float]:
    """Extracts a float metric from a string using regex."""
    if not metric_regex:
        return None
    match = re.search(metric_regex, output)
    if match:
        try:
            return float(match.group(1))
        except (ValueError, IndexError):
            print(f"  -> Warning: Regex matched but failed to parse float from '{match.group(1)}'")
            return None
    return None

def grid_search(
    param_grid: Dict[str, list],
    base_eval_cmd: str,
    metric_regex: Optional[str],
    dry_run: bool,
    max_tests: Optional[int],
):
    results = []
    param_names = list(param_grid.keys())
    value_combinations = list(itertools.product(*(param_grid[name] for name in param_names)))

    num_to_run = len(value_combinations)
    if max_tests and max_tests > 0:
        num_to_run = min(num_to_run, max_tests)
        
    print(f"Starting grid search. Total combinations to test: {num_to_run}")

    for i, combo in enumerate(value_combinations):
        if i >= num_to_run:
            break

        current_params = dict(zip(param_names, combo))
        
        # Build a string of named arguments: --cta-name1 value1 --cta-name2 value2 ...
        params_list = [f"--cta-{key.replace('_', '-')} {value}" for key, value in current_params.items()]
        params_str = " ".join(params_list)
        
        full_eval_cmd = f"{base_eval_cmd} {params_str}"

        # Print current test configuration
        param_print_str = ", ".join(f"{k}={v}" for k,v in current_params.items())
        print(f"\nTest #{i+1}/{num_to_run}: [{param_print_str}]", flush=True)

        if dry_run:
            print(f"  -> Would execute: {full_eval_cmd}")
            continue

        rc, output = run_eval_cmd(full_eval_cmd)
        metric = extract_metric(output, metric_regex)
        
        if metric is not None:
            print(f"  -> Metric: {metric}", flush=True)
        else:
            print(f"  -> Metric: N/A (not found in output)", flush=True)

        if rc != 0:
            print(f"  -> Warning: Command exited with non-zero status code: {rc}")

        results.append((current_params, metric))

    # --- Reporting Results ---
    print("\n--- Grid Search Complete ---")
    valid_results = [(p, m) for p, m in results if m is not None]
    if not valid_results:
        print("No valid metrics were collected. Cannot determine the best setup.")
        return

    # Assuming lower is better for the metric
    best_params, best_metric = min(valid_results, key=lambda item: item[1])
    
    print("\nBest setup (lowest metric):")
    print(f"  Metric = {best_metric}")
    best_params_str = ", ".join(f"{k}={v}" for k, v in best_params.items())
    print(f"  Params = {{{best_params_str}}}")

def parse_list(s: str, cast_type, arg_name: str):
    values = []
    for x in s.split(','):
        x = x.strip()
        if not x:
            continue
        try:
            values.append(cast_type(x))
        except ValueError:
            print(f"Error: Invalid value '{x}' provided for argument '{arg_name}'.", file=sys.stderr)
            print(f"Please provide a comma-separated list of {cast_type.__name__}s.", file=sys.stderr)
            sys.exit(1)
    return values

def main():
    p = argparse.ArgumentParser(description="Tune algorithm hyperparameters via command-line arguments.")
    p.add_argument("--eval-cmd", required=True, help="Base command to run (e.g., './build/app --file a.json')")
    p.add_argument("--metric-regex", required=True, help="Regex to extract numeric metric from stdout")
    p.add_argument("--dry-run", action="store_true", help="Print commands without executing them")
    p.add_argument("--max-tests", type=int, help="Limit number of tests to run")

    # Arguments for parameter values
    p.add_argument("--coalition", default="3.0,2.0,5.0", help="Comma-separated coalition distances")
    p.add_argument("--min-pts", default="7,5,9", help="Comma-separated cluster min points")
    p.add_argument("--ratio", default="0.2,0.35,0.6", help="Comma-separated cluster ratio values")
    
    args = p.parse_args()

    param_grid = {
        "coalition": parse_list(args.coalition, float, "--coalition"),
        "min_pts": parse_list(args.min_pts, int, "--min-pts"),
        "ratio": parse_list(args.ratio, float, "--ratio"),
    }

    grid_search(
        param_grid,
        args.eval_cmd,
        args.metric_regex,
        args.dry_run,
        args.max_tests,
    )

if __name__ == "__main__":
    main()