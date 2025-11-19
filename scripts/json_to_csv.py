#!/usr/bin/env python3
"""
Convert JSON measurement files to CSV format.

Usage:
    python scripts/json_to_csv.py <input.json> [output.csv]
    python scripts/json_to_csv.py Recordings/*.json  # Convert all JSON files in Recordings/

If output.csv is not specified, it will be created with the same name as input but .csv extension.
"""

import json
import csv
import sys
import os
from pathlib import Path


def json_to_csv(json_file, csv_file=None):
    """
    Convert a JSON measurements file to CSV.
    
    Args:
        json_file: Path to input JSON file
        csv_file: Path to output CSV file (optional; defaults to json_file with .csv extension)
    """
    json_path = Path(json_file)
    
    if not json_path.exists():
        print(f"Error: File not found: {json_file}", file=sys.stderr)
        return False
    
    if csv_file is None:
        csv_file = json_path.with_suffix('.csv')
    else:
        csv_file = Path(csv_file)
    
    try:
        # Read JSON
        with open(json_path, 'r') as f:
            data = json.load(f)
        
        # Extract measurements array
        if 'measurements' not in data:
            print(f"Warning: No 'measurements' key in {json_file}. Skipping.", file=sys.stderr)
            return False
        
        measurements = data['measurements']
        if not measurements:
            print(f"Warning: No measurements found in {json_file}. Skipping.", file=sys.stderr)
            return False
        
        # Get headers from first measurement
        headers = list(measurements[0].keys())
        
        # Write CSV
        with open(csv_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=headers)
            writer.writeheader()
            writer.writerows(measurements)
        
        print(f"✓ Converted: {json_file} -> {csv_file}")
        return True
    
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {json_file}: {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"Error processing {json_file}: {e}", file=sys.stderr)
        return False


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    
    input_files = sys.argv[1:-1] if len(sys.argv) > 3 else [sys.argv[1]]
    output_file = sys.argv[-1] if len(sys.argv) > 2 and not sys.argv[-1].endswith('.json') else None
    
    # Handle glob patterns
    all_files = []
    for pattern in input_files:
        if '*' in pattern or '?' in pattern:
            all_files.extend(Path('.').glob(pattern))
        else:
            all_files.append(Path(pattern))
    
    if not all_files:
        print("Error: No input files found.", file=sys.stderr)
        sys.exit(1)
    
    # If multiple input files, ignore output file parameter
    if len(all_files) > 1:
        success_count = sum(1 for f in all_files if json_to_csv(str(f)))
        print(f"\nConverted {success_count}/{len(all_files)} file(s).")
        sys.exit(0 if success_count == len(all_files) else 1)
    else:
        # Single input file
        success = json_to_csv(str(all_files[0]), output_file)
        sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
