#!/usr/bin/env python3
"""
Read variable assignments from stdin (like output of your C++ program) and plot:
 - 2D scatter of x,y with centroids and AoA arrows when present
 - 3D scatter of x,y,rssi colored by RSSI when present

Usage:
  ./build/signal-triangulation | python3 plot_from_stdin.py

Options:
  --no-show       Do not show interactive windows (only save PNGs)
  --out-prefix P  Save files as P_2d.png and P_3d.png (default: plots)

The script ignores latitude/longitude lines ("Lat=..." / "Lon=...").
"""

import sys
import re
import ast
from typing import Dict, Any
import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd

def parse_search_space_costs(text: str):
    """
    Parse the 'Search Space Costs:' section.
    Returns X, Y, Z arrays for plotting, or None if not found.
    """
    lines = text.splitlines()
    start_idx = -1
    for i, line in enumerate(lines):
        if line.strip() == "Search Space Costs:":
            start_idx = i + 1
            break
    
    if start_idx == -1:
        return None, None, None

    data = []
    for line in lines[start_idx:]:
        parts = line.split(',')
        if len(parts) == 3:
            try:
                x = float(parts[0])
                y = float(parts[1])
                cost = float(parts[2])
                data.append((x, y, cost))
            except ValueError:
                break # Stop at non-numeric line
        else:
            break
            
    if not data:
        return None, None, None

    # Convert to numpy arrays and pivot
    try:
        df = pd.DataFrame(data, columns=['x', 'y', 'cost'])
        pivot = df.pivot(index='y', columns='x', values='cost')
        X = pivot.columns.values
        Y = pivot.index.values
        X, Y = np.meshgrid(X, Y)
        Z = pivot.values
        return X, Y, Z
    except Exception as e:
        print(f"Error processing search space data: {e}")
        return None, None, None

def plot_heatmap(X, Y, Z, out_dir=None, show=True):
    fig, ax = plt.subplots(figsize=(10, 8))
    cp = ax.contourf(X, Y, Z, levels=50, cmap='viridis')
    fig.colorbar(cp, label='Cost')
    ax.set_title('Search Space Cost Landscape (Heatmap)')
    ax.set_xlabel('X (meters)')
    ax.set_ylabel('Y (meters)')
    ax.grid(True, alpha=0.3)
    
    if out_dir:
        output_file = f"{out_dir}/search_space_heatmap.png"
        fig.savefig(output_file, dpi=200)
        print(f"Saved heatmap to {output_file}")
    
    if show:
        plt.show()

def plot_3d_surface(X, Y, Z, out_dir=None, show=True):
    fig = plt.figure(figsize=(12, 9))
    ax = fig.add_subplot(111, projection='3d')
    surf = ax.plot_surface(X, Y, Z, cmap='viridis', edgecolor='none', alpha=0.8)
    fig.colorbar(surf, ax=ax, shrink=0.5, aspect=5, label='Cost')
    ax.set_title('Search Space Cost Landscape (3D Surface)')
    ax.set_xlabel('X (meters)')
    ax.set_ylabel('Y (meters)')
    ax.set_zlabel('Cost')
    
    if out_dir:
        output_file = f"{out_dir}/search_space_3d.png"
        fig.savefig(output_file, dpi=200)
        print(f"Saved 3D surface plot to {output_file}")
    
    if show:
        plt.show()

def plot_composite_heatmap(X, Y, Z, data_x, data_y, centroids=None, aoas=None, result_point=None, source_point=None, out_dir=None, show=True):
    fig, ax = plt.subplots(figsize=(12, 10))
    
    # 1. The Cost Heatmap
    # Use a lighter alpha so points show up
    cp = ax.contourf(X, Y, Z, levels=50, cmap='viridis', alpha=0.9)
    fig.colorbar(cp, label='Cost')
    
    # 2. Data Points (measurements) - subtle white dots
    if data_x is not None and data_y is not None:
        ax.scatter(data_x, data_y, c='black', s=15, alpha=0.4, label='measurements', edgecolors='none')

    # 3. Centroids and AoA
    if centroids is not None:
        cx, cy = centroids
        cx = np.array(cx)
        cy = np.array(cy)
        # Plot centroids
        ax.scatter(cx, cy, marker='X', c='red', s=120, edgecolors='white', linewidth=1.5, label='centroids', zorder=5)
        
        if aoas is not None:
            angles = np.array(aoas)
            # compute arrow length based on grid extent
            xr = np.max(X) - np.min(X)
            yr = np.max(Y) - np.min(Y)
            extent = max(xr, yr)
            arrow_len = extent * 0.1
            rads = np.deg2rad(angles)
            dxs = arrow_len * np.cos(rads)
            dys = arrow_len * np.sin(rads)
            
            # Quiver for AoA
            ax.quiver(cx, cy, dxs, dys, angles='xy', scale_units='xy', scale=1, color='red', width=0.006, headwidth=4, zorder=4)

    # 4. Result Point
    if result_point is not None:
        rx, ry = result_point
        ax.scatter([rx], [ry], marker='*', c='gold', s=250, edgecolors='black', linewidth=1.5, label='result', zorder=10)

    # 5. Source Point
    if source_point is not None:
        sx, sy = source_point
        ax.scatter([sx], [sy], marker='P', c='cyan', s=200, edgecolors='black', linewidth=1.5, label='source', zorder=10)

    ax.set_title('Composite Search Space: Cost + Geometry')
    ax.set_xlabel('X (meters)')
    ax.set_ylabel('Y (meters)')
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.legend(loc='upper right', framealpha=0.9)
    
    if out_dir:
        output_file = f"{out_dir}/composite_heatmap.png"
        fig.savefig(output_file, dpi=200)
        print(f"Saved composite heatmap to {output_file}")
    
    if show:
        plt.show()

def plot_composite_3d_surface(X, Y, Z, result_point=None, source_point=None, out_dir=None, show=True):
    fig = plt.figure(figsize=(12, 9))
    ax = fig.add_subplot(111, projection='3d')
    
    # Surface
    surf = ax.plot_surface(X, Y, Z, cmap='viridis', edgecolor='none', alpha=0.6)
    fig.colorbar(surf, ax=ax, shrink=0.5, aspect=5, label='Cost')
    
    # Helper to find Z for a given (x,y)
    def get_z(px, py):
        # X[0, :] are x coordinates, Y[:, 0] are y coordinates
        x_axis = X[0, :]
        y_axis = Y[:, 0]
        idx_x = (np.abs(x_axis - px)).argmin()
        idx_y = (np.abs(y_axis - py)).argmin()
        return Z[idx_y, idx_x]

    if result_point is not None:
        rx, ry = result_point
        try:
            rz = get_z(rx, ry)
            ax.scatter([rx], [ry], [rz], c='gold', marker='*', s=200, edgecolors='black', label='result', zorder=10)
        except Exception as e:
            print(f"Warning: Could not plot result point at ({rx}, {ry}): {e}")
        
    if source_point is not None:
        sx, sy = source_point
        try:
            sz = get_z(sx, sy)
            ax.scatter([sx], [sy], [sz], c='cyan', marker='P', s=150, edgecolors='black', label='source', zorder=10)
        except Exception as e:
            print(f"Warning: Could not plot source point at ({sx}, {sy}): {e}")

    ax.set_title('Composite 3D Cost Surface')
    ax.set_xlabel('X (meters)')
    ax.set_ylabel('Y (meters)')
    ax.set_zlabel('Cost')
    ax.legend()

    if out_dir:
        output_file = f"{out_dir}/composite_3d.png"
        fig.savefig(output_file, dpi=200)
        print(f"Saved composite 3D plot to {output_file}")
    
    if show:
        plt.show()

def parse_lists_from_text(text: str) -> Dict[str, Any]:
    """Find variable assignments of the form `name = [ ... ]` and return Python lists.
    Case-insensitive for name matching; keys returned in lower-case.
    """
    result = {}
    # regex to find name = [ ... ] (non-greedy inside brackets)
    pattern = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(\[[^\]]*\])", re.S)
    for m in pattern.finditer(text):
        name = m.group(1)
        raw = m.group(2)
        # try to safely evaluate the list
        try:
            # ast.literal_eval can parse trailing commas fine
            value = ast.literal_eval(raw)
            result[name.lower()] = value
        except Exception:
            # fallback: try to cleanup (remove trailing commas before ])
            cleaned = re.sub(r",\s*\]", "]", raw)
            try:
                value = ast.literal_eval(cleaned)
                result[name.lower()] = value
            except Exception:
                # skip if unparsable
                continue
    return result


def parse_clusters_from_text(text: str):
    """Parse cluster blocks in the newer program output.

    Expected header lines like:
      Cluster 0: centroid_x: -3.63872, centroid_y: 3.54872, avg_rssi: -67, estimated_aoa: 78.7988, ratio: 0.618537, num_points: 7
    Followed by lines starting with `p <x> <y>` for the cluster points. Blank lines or next header end the cluster.

    Returns a list of dicts with keys: id, centroid_x, centroid_y, avg_rssi, estimated_aoa, ratio, num_points, points
    """
    clusters = []
    cur = None
    cur_points = []
    header_re = re.compile(r"^\s*Cluster\s+(\d+)\s*:\s*(.*)$")
    kv_re = re.compile(r"([A-Za-z_]+)\s*:\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)")

    for raw in text.splitlines():
        line = raw.rstrip()
        if not line.strip():
            # blank line ends current cluster
            if cur is not None:
                cur['points'] = cur_points
                clusters.append(cur)
                cur = None
                cur_points = []
            continue

        m = header_re.match(line)
        if m:
            # commit previous cluster if any
            if cur is not None:
                cur['points'] = cur_points
                clusters.append(cur)
                cur_points = []
            cid = int(m.group(1))
            rest = m.group(2)
            cur = {'id': cid, 'centroid_x': 0.0, 'centroid_y': 0.0,
                   'avg_rssi': None, 'estimated_aoa': None, 'ratio': None, 'num_points': None}
            for kv in kv_re.finditer(rest):
                k = kv.group(1)
                v = kv.group(2)
                try:
                    fv = float(v)
                except Exception:
                    continue
                if k == 'centroid_x': cur['centroid_x'] = fv
                elif k == 'centroid_y': cur['centroid_y'] = fv
                elif k == 'avg_rssi': cur['avg_rssi'] = fv
                elif k == 'estimated_aoa': cur['estimated_aoa'] = fv
                elif k == 'ratio': cur['ratio'] = fv
                elif k == 'num_points': cur['num_points'] = int(fv)
            continue

        # point line: starts with 'p ' then x y
        if line.lstrip().startswith('p '):
            parts = line.split()
            if len(parts) >= 3 and cur is not None:
                try:
                    x = float(parts[1]); y = float(parts[2])
                    cur_points.append((x, y))
                except Exception:
                    continue

    if cur is not None:
        cur['points'] = cur_points
        clusters.append(cur)
    return clusters


def parse_datapoints_from_text(text: str):
    """Parse the new 'Data Points:' section where each data point is a line like:
    "  x: 1.4617, y: 7.12916, rssi: -63"

    Returns lists (x_list, y_list, rssi_list).
    """
    pts = []
    # locate the Data Points: section start
    start = None
    for i, raw in enumerate(text.splitlines()):
        if raw.strip().lower().startswith('data points:'):
            start = i + 1
            break
    if start is None:
        return None, None, None

    dp_re = re.compile(r"x\s*:\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)\s*,\s*y\s*:\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)\s*,\s*rssi\s*:\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)")
    for raw in text.splitlines()[start:]:
        if not raw.strip():
            break
        m = dp_re.search(raw)
        if m:
            try:
                x = float(m.group(1)); y = float(m.group(2)); r = float(m.group(3))
                pts.append((x, y, r))
            except Exception:
                continue
        else:
            # stop if we reach the Clusters: section
            if raw.strip().lower().startswith('clusters:'):
                break

    if not pts:
        return None, None, None
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    rss = [p[2] for p in pts]
    return xs, ys, rss


def extract_resulting_point(text: str):
    """Extract a resulting point from lines like:
    "Resulting point after gradient descent: x=-19.2211, y=-40.8592"
    Returns (x, y) as floats or None if not found.
    """
    # Try multiple variants, be permissive about spacing and wording
    patterns = [
        r"Resulting point[^\n\r]*x\s*=\s*([+-]?\d+\.?\d*)\s*,\s*y\s*=\s*([+-]?\d+\.?\d*)",
        r"Resulting point[^\n\r]*:\s*x\s*=\s*([+-]?\d+\.?\d*)\s*,\s*y\s*=\s*([+-]?\d+\.?\d*)",
        r"x\s*=\s*([+-]?\d+\.?\d*)\s*,\s*y\s*=\s*([+-]?\d+\.?\d*)\s*#?\s*Resulting",
    ]
    for pat in patterns:
        m = re.search(pat, text, re.IGNORECASE)
        if m:
            try:
                xv = float(m.group(1))
                yv = float(m.group(2))
                return (xv, yv)
            except Exception:
                continue
    return None


def extract_source_point_from_text(text: str):
    """Look for a line like:
    'Source position from file: x=-0.5422806643, y=-11.24231094' or without comma.
    Return (x, y) in projected coordinates if found, else None.
    """
    patterns = [
        r"Source position from file:\s*x\s*=\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)\s*,?\s*y\s*=\s*([+-]?\d+\.?\d*(?:[eE][+-]?\d+)?)",
    ]
    for pat in patterns:
        m = re.search(pat, text)
        if m:
            try:
                xv = float(m.group(1))
                yv = float(m.group(2))
                return (xv, yv)
            except Exception:
                continue
    return None


def plot_2d(x, y, centroids=None, aoas=None, result_point=None, out_dir=None, show=True, source_point=None):
    fig, ax = plt.subplots(figsize=(8, 6))
    ax.scatter(x, y, c='C0', label='points')
    ax.plot(x, y, c='C1', linestyle='-', linewidth=1, label='path')
    ax.set_xlabel('x (meters)')
    ax.set_ylabel('y (meters)')
    ax.set_title('Signal measurement points (x, y)')
    ax.grid(True, linestyle='--', alpha=0.5)
    ax.set_aspect('equal', adjustable='box')

    if centroids is not None:
        cx, cy = centroids
        cx = np.array(cx)
        cy = np.array(cy)
        ax.scatter(cx, cy, marker='X', c='C3', s=80, label='centroids')
        if aoas is not None:
            angles = np.array(aoas)
            # compute arrow length relative to data extent
            xr = max(x) - min(x) if len(x) > 1 else 1.0
            yr = max(y) - min(y) if len(y) > 1 else 1.0
            extent = max(xr, yr)
            arrow_len = extent * 0.08
            rads = np.deg2rad(angles)
            dxs = arrow_len * np.cos(rads)
            dys = arrow_len * np.sin(rads)
            ax.quiver(cx, cy, dxs, dys, angles='xy', scale_units='xy', scale=1, color='C3', width=0.005)

    # plot resulting gradient-descent point if provided
    if result_point is not None:
        rx, ry = result_point
        ax.scatter([rx], [ry], marker='*', c='gold', s=140, label='result')

    # plot source position if provided
    if source_point is not None:
        sx, sy = source_point
        ax.scatter([sx], [sy], marker='P', c='red', s=120, label='source')

    ax.legend()
    if out_dir:
        fig.savefig(f"{out_dir}/plot_2d.png", dpi=200)
        print(f"Saved 2D plot to {out_dir}/plot_2d.png")
    if show:
        plt.show()


def plot_3d(x, y, rssi, result_point=None, out_dir=None, show=True, cmap='viridis', source_point=None):
    xs = np.array(x)
    ys = np.array(y)
    zs = np.array(rssi)

    fig = plt.figure(figsize=(9, 7))
    ax = fig.add_subplot(111, projection='3d')

    p = ax.scatter(xs, ys, zs, c=zs, cmap=cmap, depthshade=True)
    ax.plot(xs, ys, zs, color='gray', linewidth=0.8, alpha=0.6)

    ax.set_xlabel('x (meters)')
    ax.set_ylabel('y (meters)')
    ax.set_zlabel('RSSI (dBm)')
    ax.set_title('3D plot of x, y and RSSI')

    fig.colorbar(p, ax=ax, label='RSSI (dBm)')

    if result_point is not None:
        rx, ry = result_point
        # draw a vertical line at (rx, ry) from min(zs) to max(zs)
        zmin = float(np.min(zs))
        zmax = float(np.max(zs))
        ax.plot([rx, rx], [ry, ry], [zmin, zmax], color='gold', linewidth=2, alpha=0.9, label='result')
        # also mark the intersection at mean z
        zmid = (zmin + zmax) / 2.0
        ax.scatter([rx], [ry], [zmid], c='gold', marker='*', s=80)
        try:
            ax.legend()
        except Exception:
            # It is safe to ignore legend errors; legend is optional and plot remains usable.
            pass
    # plot source position if provided (as vertical marker)
    if source_point is not None:
        sx, sy = source_point
        # draw a vertical line at (rx, ry) from min(zs) to max(zs)
        zmin = float(np.min(zs))
        zmax = float(np.max(zs))
        ax.plot([sx, sx], [sy, sy], [zmin, zmax], color='red', linewidth=2, alpha=0.9, label='source')
        # also mark the intersection at mean z
        zmid = (zmin + zmax) / 2.0
        ax.scatter([sx], [sy], [zmid], c='red', marker='P', s=80)
        try:
            ax.legend()
        except Exception:
            # It is safe to ignore legend errors; legend is optional and plot remains usable.
            pass
    # vertical line at resulting point (if provided)
    if out_dir:
        fig.savefig(f"{out_dir}/plot_3d.png", dpi=200)
        print(f"Saved 3D plot to {out_dir}/plot_3d.png")
    if show:
        plt.show()

def plot_clusters(clusters, result_point=None, out_dir=None, show=True, source_point=None):
    # create a simple clusters-only plot saved to disk
        try:
            fig, ax = plt.subplots(figsize=(8, 8))
            colors = plt.get_cmap('tab10')
            xs = []
            ys = []
            for cidx, c in enumerate(clusters):
                pts = c.get('points', [])
                if pts:
                    px = [p[0] for p in pts]
                    py = [p[1] for p in pts]
                    xs.extend(px); ys.extend(py)
                    ax.scatter(px, py, color=colors(cidx % 10), alpha=0.7, label=f'cluster {c.get("id")}')
                cx = c.get('centroid_x', 0.0)
                cy = c.get('centroid_y', 0.0)
                ax.scatter([cx], [cy], marker='x', color='k')
                ax.text(cx, cy, f'c{c.get("id")} r={c.get("ratio",0.0):.2f}', fontsize=9)
            if result_point is not None:
                rx, ry = result_point
                ax.scatter([rx], [ry], marker='*', c='gold', s=140, label='result')
            if source_point is not None:
                try:
                    sx, sy = source_point
                    ax.scatter([sx], [sy], marker='P', c='red', s=120, label='source')
                except Exception:
                    pass
            if xs and ys:
                ax.set_aspect('equal', adjustable='box')
            ax.set_xlabel('X (meters)')
            ax.set_ylabel('Y (meters)')
            ax.grid(True)
            ax.legend(loc='best')
            fig.tight_layout()
            if out_dir is not None:
                fig.savefig(f"{out_dir}/plot_clusters.png", dpi=150)
                print(f"Saved clusters plot to {out_dir}/plot_clusters.png")
            # plot source on cluster plot if available
            if show:
                plt.show()
        except Exception as e:
            print(f"Failed to generate clusters plot: {e}", file=sys.stderr)


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Parse arrays from stdin and plot.')
    parser.add_argument('--no-show', action='store_true', help='Do not show interactive windows')
    parser.add_argument('--out-dir', default=None, help='Output directory or prefix for saved plots (default: don\'t save)')
    parser.add_argument('--cmap', default='viridis', help='Colormap for 3D RSSI plot')
    parser.add_argument('--save-images', action='store_true', help='Save images to directory images/ if not specified otherwise')
    args = parser.parse_args()

    # read stdin fully
    text = sys.stdin.read()
    if not text:
        print('No input received on stdin. Expecting variable assignments like `x = [..]` or cluster blocks.')
        return

    # Parse new Data Points block (preferred)
    x, y, rssi = parse_datapoints_from_text(text)
    if x is None or y is None:
        print('Missing Data Points section in input; nothing to plot.')
        return

    # Parse clusters in the new format
    clusters = parse_clusters_from_text(text)
    clusterx = [c.get('centroid_x') for c in clusters] if clusters else None
    clustery = [c.get('centroid_y') for c in clusters] if clusters else None
    aoas = [c.get('estimated_aoa') for c in clusters] if clusters else None

    show = not args.no_show
    if args.out_dir is None and args.save_images:
        args.out_dir = "images"

    # 2D plot
    centroids = None
    if clusterx is not None and clustery is not None:
        centroids = (clusterx, clustery)

    # try to extract resulting point from free-form text
    result_point = extract_resulting_point(text)

    source_point = extract_source_point_from_text(text)

    # Always save the core plots
    plot_2d(x, y, centroids=centroids, aoas=aoas, result_point=result_point, out_dir=args.out_dir, show=show, source_point=source_point)

    # 3D plot if rssi present
    if rssi is not None:
        plot_3d(x, y, rssi, result_point=result_point, out_dir=args.out_dir, show=show, cmap=args.cmap, source_point=source_point)
    # Extract source position if printed in stdout from the app

    # Parse and plot cluster-specific output (if present in stdin)
    if clusters:
        plot_clusters(clusters, result_point=result_point, out_dir=args.out_dir, show=show, source_point=source_point)

    # Parse and plot search space costs (if present)
    X, Y, Z = parse_search_space_costs(text)
    if X is not None:
        plot_heatmap(X, Y, Z, out_dir=args.out_dir, show=show)
        plot_3d_surface(X, Y, Z, out_dir=args.out_dir, show=show)
        
        # Composite plots
        plot_composite_heatmap(X, Y, Z, x, y, centroids=centroids, aoas=aoas, result_point=result_point, source_point=source_point, out_dir=args.out_dir, show=show)
        plot_composite_3d_surface(X, Y, Z, result_point=result_point, source_point=source_point, out_dir=args.out_dir, show=show)

if __name__ == '__main__':
    main()
