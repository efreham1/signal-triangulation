#!/usr/bin/env python3
"""
Hyperparameter tuner for the triangulation algorithm.

Usage with integration tests (evaluates ALL recordings):
  python scripts/tune_hyperparams.py \
    --eval-cmd "./build/tests/integration_tests --gtest_filter=Triangulation.GlobalSummary" \
    --metric-regex "Global Average Error:\\s*([0-9.]+)" \
    --coalition-distance "1.0,2.0,3.0,4.0" \
    --cluster-min-points "3,4,5" \
    --max-tests 50

Or for a single file:
  python scripts/tune_hyperparams.py \
    --eval-cmd "./build/tests/integration_tests --gtest_filter=Triangulation.SingleFileErrorCheck --run-single-file recordings/football.json" \
    --metric-regex "Global Average Error:\\s*([0-9.]+)" \
    --coalition-distance "1.0,2.0,3.0,4.0"
"""

from __future__ import annotations
import argparse
import itertools
import subprocess
import re
import sys
import json
import random
import signal
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass

# Try to import yaml, fall back gracefully
try:
    import yaml
    YAML_AVAILABLE = True
except ImportError:
    YAML_AVAILABLE = False


# Global flag for graceful shutdown
_interrupted = False
_results: List[Tuple[Dict[str, Any], Optional[float]]] = []
_minimize = True


def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully."""
    global _interrupted
    if _interrupted:
        # Second Ctrl+C, force exit
        print("\n\nForced exit.")
        sys.exit(1)
    _interrupted = True
    print("\n\nInterrupted! Finishing current test and reporting results...")


# Register signal handler
signal.signal(signal.SIGINT, signal_handler)


@dataclass
class ParamSpec:
    """Specification for a single parameter's search space."""
    name: str
    values: List[Any]
    param_type: str  # 'float', 'int', 'bool'
    
    @classmethod
    def from_list(cls, name: str, values: List[Any]) -> 'ParamSpec':
        """Create from explicit list of values."""
        if all(isinstance(v, bool) for v in values):
            return cls(name, values, 'bool')
        elif all(isinstance(v, int) for v in values):
            return cls(name, values, 'int')
        else:
            return cls(name, [float(v) for v in values], 'float')
    
    @classmethod
    def from_range(cls, name: str, start: float, end: float, step: float, param_type: str = 'float') -> 'ParamSpec':
        """Create from range specification."""
        values = []
        current = start
        while current <= end + 1e-9:  # epsilon for float comparison
            if param_type == 'int':
                values.append(int(round(current)))
            else:
                values.append(round(current, 6))
            current += step
        return cls(name, values, param_type)
    
    @classmethod
    def from_logspace(cls, name: str, start: float, end: float, num_points: int) -> 'ParamSpec':
        """Create logarithmically spaced values (useful for weights, learning rates)."""
        import math
        log_start = math.log10(start)
        log_end = math.log10(end)
        step = (log_end - log_start) / (num_points - 1)
        values = [round(10 ** (log_start + i * step), 6) for i in range(num_points)]
        return cls(name, values, 'float')


# Default search spaces for each parameter
DEFAULT_SEARCH_SPACES: Dict[str, ParamSpec] = {
    # Clustering
    'coalition_distance': ParamSpec.from_range('coalition_distance', 1.0, 5.0, 1.0),
    'cluster_min_points': ParamSpec.from_range('cluster_min_points', 3, 7, 1, 'int'),
    'cluster_ratio_threshold': ParamSpec.from_list('cluster_ratio_threshold', [0.15, 0.25, 0.35, 0.5]),
    'max_internal_distance': ParamSpec.from_range('max_internal_distance', 10, 30, 5, 'int'),
    
    # Geometric
    'min_geometric_ratio': ParamSpec.from_list('min_geometric_ratio', [0.1, 0.15, 0.2, 0.25]),
    'ideal_geometric_ratio': ParamSpec.from_list('ideal_geometric_ratio', [0.8, 1.0, 1.2]),
    'min_area': ParamSpec.from_list('min_area', [5.0, 10.0, 20.0, 50.0]),
    'ideal_area': ParamSpec.from_list('ideal_area', [25.0, 50.0, 100.0, 200.0]),
    'max_area': ParamSpec.from_list('max_area', [500.0, 1000.0, 2000.0]),
    
    # RSSI
    'min_rssi_variance': ParamSpec.from_list('min_rssi_variance', [3.0, 5.0, 8.0, 12.0]),
    'bottom_rssi': ParamSpec.from_list('bottom_rssi', [-95.0, -90.0, -85.0, -80.0]),
    
    # Overlap
    'max_overlap': ParamSpec.from_list('max_overlap', [0.0, 0.05, 0.1, 0.2]),
    
    # Weights (log scale makes sense for weights)
    'weight_geometric_ratio': ParamSpec.from_logspace('weight_geometric_ratio', 0.1, 10.0, 5),
    'weight_area': ParamSpec.from_logspace('weight_area', 0.1, 10.0, 5),
    'weight_rssi_variance': ParamSpec.from_logspace('weight_rssi_variance', 0.1, 10.0, 5),
    'weight_rssi': ParamSpec.from_logspace('weight_rssi', 0.1, 10.0, 5),
    'extra_weight': ParamSpec.from_list('extra_weight', [0.0, 0.5, 1.0, 2.0]),
    
    # Timing
    'per_seed_timeout': ParamSpec.from_list('per_seed_timeout', [0.5, 1.0, 2.0, 5.0]),
    
    # Grid
    'grid_half_size': ParamSpec.from_list('grid_half_size', [250, 500, 750, 1000]),
}


def run_eval_cmd(cmd: str, timeout: Optional[float] = None) -> Tuple[int, str]:
    """Executes the provided shell command and captures its output."""
    try:
        proc = subprocess.run(
            cmd, 
            shell=True, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.STDOUT, 
            text=True, 
            check=False,
            timeout=timeout
        )
        return proc.returncode, proc.stdout
    except subprocess.TimeoutExpired:
        return -1, "TIMEOUT"


def extract_metric(output: str, metric_regex: str) -> Optional[float]:
    """Extracts a float metric from a string using regex.
    
    Returns None if:
    - No metric found
    - Any file failed to produce output
    """
    # Check for failed files first (handles [DEBUG] prefix)
    if re.search(r"No output from app for file:", output):
        return None
    
    match = re.search(metric_regex, output)
    if match:
        try:
            return float(match.group(1))
        except (ValueError, IndexError):
            return None
    return None


def load_search_space_from_yaml(yaml_path: str) -> Dict[str, ParamSpec]:
    """Load search space from YAML file."""
    if not YAML_AVAILABLE:
        print("Error: PyYAML not installed. Run: pip install pyyaml", file=sys.stderr)
        sys.exit(1)
    
    with open(yaml_path, 'r') as f:
        config = yaml.safe_load(f)
    
    search_space = {}
    for name, spec in config.get('parameters', {}).items():
        if 'values' in spec:
            search_space[name] = ParamSpec.from_list(name, spec['values'])
        elif 'range' in spec:
            r = spec['range']
            param_type = spec.get('type', 'float')
            search_space[name] = ParamSpec.from_range(
                name, r['start'], r['end'], r['step'], param_type
            )
        elif 'logspace' in spec:
            ls = spec['logspace']
            search_space[name] = ParamSpec.from_logspace(
                name, ls['start'], ls['end'], ls['num_points']
            )
    
    return search_space


def build_command(base_cmd: str, params: Dict[str, Any]) -> str:
    """Build full command with parameters."""
    param_args = []
    for name, value in params.items():
        cli_name = f"{name.replace('_', '-')}"
        param_args.append(f"--{cli_name} {value}")
    return f"{base_cmd} {' '.join(param_args)}"


def grid_search(
    search_space: Dict[str, ParamSpec],
    base_cmd: str,
    metric_regex: str,
    max_tests: Optional[int],
    dry_run: bool,
    cmd_timeout: Optional[float],
) -> List[Tuple[Dict[str, Any], Optional[float]]]:
    """Exhaustive grid search over all combinations."""
    global _interrupted, _results
    _results = []
    
    param_names = list(search_space.keys())
    all_values = [search_space[name].values for name in param_names]
    all_combinations = list(itertools.product(*all_values))
    
    if max_tests and max_tests > 0:
        all_combinations = all_combinations[:max_tests]
    
    total = len(all_combinations)
    print(f"Grid Search: {total} combinations to test")
    print(f"Parameters: {param_names}")
    print("(Press Ctrl+C to stop early and see results so far)")
    print()
    
    for i, combo in enumerate(all_combinations):
        if _interrupted:
            print(f"\nStopped after {i} tests.")
            break
            
        params = dict(zip(param_names, combo))
        cmd = build_command(base_cmd, params)
        
        param_str = ", ".join(f"{k}={v}" for k, v in params.items())
        print(f"[{i+1}/{total}] {param_str}")
        
        if dry_run:
            print(f"  CMD: {cmd}")
            continue
        
        rc, output = run_eval_cmd(cmd, cmd_timeout)
        
        # Check for failed files
        if re.search(r"No output from app for file:", output):
            print(f"  -> INVALID: Some files failed to produce output")
            _results.append((params, None))
            continue
        
        metric = extract_metric(output, metric_regex)
        
        if metric is not None:
            print(f"  -> Metric: {metric:.4f}")
        else:
            print(f"  -> Metric: N/A (rc={rc})")
        
        _results.append((params, metric))
    
    return _results


def random_search(
    search_space: Dict[str, ParamSpec],
    base_cmd: str,
    metric_regex: str,
    num_samples: int,
    dry_run: bool,
    cmd_timeout: Optional[float],
) -> List[Tuple[Dict[str, Any], Optional[float]]]:
    """Random sampling from search space."""
    global _interrupted, _results
    _results = []
    
    param_names = list(search_space.keys())
    print(f"Random Search: {num_samples} samples")
    print(f"Parameters: {param_names}")
    print("(Press Ctrl+C to stop early and see results so far)")
    print()
    
    for i in range(num_samples):
        if _interrupted:
            print(f"\nStopped after {i} tests.")
            break
            
        params = {name: random.choice(search_space[name].values) for name in param_names}
        cmd = build_command(base_cmd, params)
        
        param_str = ", ".join(f"{k}={v}" for k, v in params.items())
        print(f"[{i+1}/{num_samples}] {param_str}")
        
        if dry_run:
            print(f"  CMD: {cmd}")
            continue
        
        rc, output = run_eval_cmd(cmd, cmd_timeout)
        
        # Check for failed files
        if re.search(r"No output from app for file:", output):
            print(f"  -> INVALID: Some files failed to produce output")
            
            failed_files = re.findall(r"No output from app for file:\s*(\S+)", output)
            if failed_files:
                print("Failed files:")
                for f in failed_files:
                    print(f"  {f}")
            _results.append((params, None))
            continue
        
        metric = extract_metric(output, metric_regex)
        
        if metric is not None:
            print(f"  -> Metric: {metric:.4f}")
        else:
            print(f"  -> Metric: N/A (rc={rc})")
        
        _results.append((params, metric))
    
    return _results

def report_results(results: List[Tuple[Dict[str, Any], Optional[float]]], minimize: bool = True):
    """Print summary of results."""
    print("\n" + "=" * 60)
    print("RESULTS SUMMARY")
    if _interrupted:
        print("(Interrupted - partial results)")
    print("=" * 60)
    
    valid_results = [(p, m) for p, m in results if m is not None]
    
    if not valid_results:
        print("No valid results collected.")
        return
    
    # Sort by metric
    sorted_results = sorted(valid_results, key=lambda x: x[1], reverse=not minimize)
    
    # Best result
    best_params, best_metric = sorted_results[0]
    best_args = " ".join(f"--{k.replace('_', '-')} {v}" for k, v in best_params.items())
    
    print(f"\nBest {'(lowest)' if minimize else '(highest)'} metric: {best_metric:.6f}")
    print(f"Full Command: ./build/tests/integration_tests --gtest_filter=Triangulation.GlobalSummary {best_args}")
    
    # Top 5
    num_to_show = min(5, len(sorted_results))
    print(f"\nTop {num_to_show} configurations:")
    for i, (params, metric) in enumerate(sorted_results[:num_to_show]):
        param_str = " ".join(f"--{k.replace('_', '-')} {v}" for k, v in params.items())
        print(f"  {i+1}. {metric:.6f} | {param_str}")
    
    # Statistics
    metrics = [m for _, m in valid_results]
    print(f"\nStatistics ({len(valid_results)} valid runs):")
    print(f"  Min:    {min(metrics):.6f}")
    print(f"  Max:    {max(metrics):.6f}")
    print(f"  Mean:   {sum(metrics)/len(metrics):.6f}")
    print(f"  Median: {sorted(metrics)[len(metrics)//2]:.6f}")

def parse_param_list(value: str, param_type: str = 'float') -> List[Any]:
    """Parse comma-separated parameter values."""
    values = []
    for v in value.split(','):
        v = v.strip()
        if not v:
            continue
        if param_type == 'int':
            values.append(int(float(v)))  # Handle "10.0" -> 10
        elif param_type == 'bool':
            values.append(v.lower() in ('true', '1', 'yes'))
        else:
            values.append(float(v))
    return values


def main():
    global _minimize
    
    p = argparse.ArgumentParser(
        description="Hyperparameter tuner for triangulation algorithm",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Grid search with inline parameters
  %(prog)s --eval-cmd "./build/tests/integration_tests --gtest_filter=Triangulation.GlobalSummary" \\
           --metric-regex "Global Average Error:\\s*([0-9.]+)" \\
           --coalition-distance "1,2,3,4" \\
           --cluster-min-points "3,4,5"

  # Random search with YAML config
  %(prog)s --eval-cmd "./build/tests/integration_tests --gtest_filter=Triangulation.GlobalSummary" \\
           --metric-regex "Global Average Error:\\s*([0-9.]+)" \\
           --search-space config.yaml \\
           --search-mode random --max-tests 100

  # Dry run to see commands
  %(prog)s --eval-cmd "./build/signal-triangulation -s rec.json" \\
           --metric-regex "error" --dry-run
           
Press Ctrl+C during execution to stop early and see results collected so far.
        """
    )
    
    # Required
    p.add_argument("--eval-cmd", required=True, help="Base command to execute")
    p.add_argument("--metric-regex", required=True, help="Regex to extract metric (group 1)")
    
    # Search configuration
    p.add_argument("--search-space", help="YAML file with search space definition")
    p.add_argument("--search-mode", choices=['grid', 'random'], default='grid',
                   help="Search strategy (default: grid)")
    p.add_argument("--max-tests", type=int, help="Maximum number of tests to run")
    p.add_argument("--cmd-timeout", type=float, default=300, help="Timeout per command (seconds)")
    p.add_argument("--maximize", action="store_true", help="Maximize metric instead of minimize")
    p.add_argument("--dry-run", action="store_true", help="Print commands without executing")
    
    # Inline parameter definitions (override defaults)
    p.add_argument("--coalition-distance", help="Values for coalition_distance")
    p.add_argument("--cluster-min-points", help="Values for cluster_min_points")
    p.add_argument("--cluster-ratio-threshold", help="Values for cluster_ratio_threshold")
    p.add_argument("--max-internal-distance", help="Values for max_internal_distance")
    p.add_argument("--min-geometric-ratio", help="Values for min_geometric_ratio")
    p.add_argument("--ideal-geometric-ratio", help="Values for ideal_geometric_ratio")
    p.add_argument("--min-area", help="Values for min_area")
    p.add_argument("--ideal-area", help="Values for ideal_area")
    p.add_argument("--max-area", help="Values for max_area")
    p.add_argument("--min-rssi-variance", help="Values for min_rssi_variance")
    p.add_argument("--bottom-rssi", help="Values for bottom_rssi")
    p.add_argument("--max-overlap", help="Values for max_overlap")
    p.add_argument("--weight-geometric-ratio", help="Values for weight_geometric_ratio")
    p.add_argument("--weight-area", help="Values for weight_area")
    p.add_argument("--weight-rssi-variance", help="Values for weight_rssi_variance")
    p.add_argument("--weight-rssi", help="Values for weight_rssi")
    p.add_argument("--extra-weight", help="Values for extra_weight")
    p.add_argument("--per-seed-timeout", help="Values for per_seed_timeout")
    p.add_argument("--grid-half-size", help="Values for grid_half_size")
    
    args = p.parse_args()
    _minimize = not args.maximize
    
    # Build search space
    search_space: Dict[str, ParamSpec] = {}
    
    # Load from YAML if provided
    if args.search_space:
        search_space = load_search_space_from_yaml(args.search_space)
    
    # Override with inline parameters
    param_mapping = {
        'coalition_distance': ('coalition-distance', 'float'),
        'cluster_min_points': ('cluster-min-points', 'int'),
        'cluster_ratio_threshold': ('cluster-ratio-threshold', 'float'),
        'max_internal_distance': ('max-internal-distance', 'int'),
        'min_geometric_ratio': ('min-geometric-ratio', 'float'),
        'ideal_geometric_ratio': ('ideal-geometric-ratio', 'float'),
        'min_area': ('min-area', 'float'),
        'ideal_area': ('ideal-area', 'float'),
        'max_area': ('max-area', 'float'),
        'min_rssi_variance': ('min-rssi-variance', 'float'),
        'bottom_rssi': ('bottom-rssi', 'float'),
        'max_overlap': ('max-overlap', 'float'),
        'weight_geometric_ratio': ('weight-geometric-ratio', 'float'),
        'weight_area': ('weight-area', 'float'),
        'weight_rssi_variance': ('weight-rssi-variance', 'float'),
        'weight_rssi': ('weight-rssi', 'float'),
        'extra_weight': ('extra-weight', 'float'),
        'per_seed_timeout': ('per-seed-timeout', 'float'),
        'grid_half_size': ('grid-half-size', 'int'),
    }
    
    for param_name, (arg_name, param_type) in param_mapping.items():
        arg_value = getattr(args, arg_name.replace('-', '_'), None)
        if arg_value:
            values = parse_param_list(arg_value, param_type)
            search_space[param_name] = ParamSpec.from_list(param_name, values)
    
    if not search_space:
        print("Error: No parameters specified. Use --search-space or --<param-name> arguments.")
        print("Example: --coalition-distance '1,2,3,4' --cluster-min-points '3,4,5'")
        sys.exit(1)
    
    # Run search
    try:
        if args.search_mode == 'grid':
            results = grid_search(
                search_space, args.eval_cmd, args.metric_regex,
                args.max_tests, args.dry_run, args.cmd_timeout
            )
        else:
            num_samples = args.max_tests or 100
            results = random_search(
                search_space, args.eval_cmd, args.metric_regex,
                num_samples, args.dry_run, args.cmd_timeout
            )
    except Exception as e:
        print(f"\nError during search: {e}")
        results = _results  # Use whatever we collected
    
    if not args.dry_run:
        report_results(results, minimize=_minimize)


if __name__ == "__main__":
    main()