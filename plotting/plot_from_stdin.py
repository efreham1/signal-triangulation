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
    """Parse compact cluster blocks emitted by the C++ tool.

    Expected format:
      cluster:<id> centroid_x:<cx> centroid_y:<cy> ratio:<r>
      p <x> <y>

    Blank line separates clusters.
    Returns a list of dicts: {id, centroid_x, centroid_y, ratio, points:[(x,y), ...]}
    """
    clusters = []
    cur = None
    cur_points = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            if cur is not None:
                cur['points'] = cur_points
                clusters.append(cur)
                cur = None
                cur_points = []
            continue
        if line.startswith('cluster:'):
            tokens = line.split()
            cur = {'id': -1, 'centroid_x': 0.0, 'centroid_y': 0.0, 'ratio': 0.0}
            try:
                first = tokens[0]
                _, cid = first.split(':', 1)
                cur['id'] = int(cid)
            except Exception:
                cur['id'] = -1
            for tok in tokens[1:]:
                if ':' not in tok:
                    continue
                k, v = tok.split(':', 1)
                if k in ('centroid_x', 'centroid_y', 'ratio'):
                    try:
                        cur[k] = float(v)
                    except Exception:
                        cur[k] = 0.0
        elif line.startswith('p '):
            parts = line.split()
            if len(parts) >= 3:
                try:
                    x = float(parts[1]); y = float(parts[2])
                    cur_points.append((x, y))
                except Exception:
                    continue
    if cur is not None:
        cur['points'] = cur_points
        clusters.append(cur)
    return clusters


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


def plot_2d(x, y, centroids=None, aoas=None, result_point=None, out_dir=None, show=True):
    import matplotlib.pyplot as plt
    import numpy as np

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

    ax.legend()
    if out_dir:
        fig.savefig(f"{out_dir}/plot_2d.png", dpi=200)
        print(f"Saved 2D plot to {out_dir}/plot_2d.png")
    if show:
        plt.show()


def plot_3d(x, y, rssi, result_point=None, out_dir=None, show=True, cmap='viridis'):
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
    import numpy as np

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
        ax.plot([rx, rx], [ry, ry], [zmin, zmax], color='red', linewidth=2, alpha=0.9, label='result_line')
        # also mark the intersection at mean z
        zmid = (zmin + zmax) / 2.0
        ax.scatter([rx], [ry], [zmid], c='red', marker='*', s=80)
        try:
            ax.legend()
        except Exception:
            # It is safe to ignore legend errors; legend is optional and plot remains usable.
            pass
    if out_dir:
        fig.savefig(f"{out_dir}/plot_3d.png", dpi=200)
        print(f"Saved 3D plot to {out_dir}/plot_3d.png")
    # vertical line at resulting point (if provided)
    if show:
        plt.show()


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Parse arrays from stdin and plot.')
    parser.add_argument('--no-show', action='store_true', help='Do not show interactive windows')
    parser.add_argument('--out-dir', default=None, help='Output directory or prefix for saved plots (default: don\'t save)')
    parser.add_argument('--cmap', default='viridis', help='Colormap for 3D RSSI plot')
    args = parser.parse_args()

    # read stdin fully
    text = sys.stdin.read()
    if not text:
        print('No input received on stdin. Expecting variable assignments like `x = [..]` or cluster blocks.')
        return

    parsed = parse_lists_from_text(text)
    # map expected variables
    x = parsed.get('x') or parsed.get('xs')
    y = parsed.get('y') or parsed.get('ys')
    rssi = parsed.get('rssi') or parsed.get('rssis')
    clusterx = parsed.get('clusterx') or parsed.get('clusterx')
    clustery = parsed.get('clustery') or parsed.get('clustery')
    aoas = parsed.get('aoas') or parsed.get('aoa')

    if x is None or y is None:
        print('Missing x or y arrays in input; nothing to plot.')
        return

    show = not args.no_show

    # 2D plot
    centroids = None
    if clusterx is not None and clustery is not None:
        centroids = (clusterx, clustery)
    # try to extract resulting point from free-form text
    result_point = extract_resulting_point(text)
    if result_point is not None:
        print(f"Parsed resulting point: x={result_point[0]}, y={result_point[1]}")

    # Always save the core plots
    plot_2d(x, y, centroids=centroids, aoas=aoas, result_point=result_point, out_dir=args.out_dir, show=show)

    # 3D plot if rssi present
    if rssi is not None:
        plot_3d(x, y, rssi, result_point=result_point, out_dir=args.out_dir, show=show, cmap=args.cmap)
    # Parse and plot cluster-specific output (if present in stdin)
    clusters = parse_clusters_from_text(text)
    if clusters:
        # create a simple clusters-only plot saved to disk
        try:
            import matplotlib.pyplot as plt
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
            if xs and ys:
                ax.set_aspect('equal', adjustable='box')
            ax.set_xlabel('X (meters)')
            ax.set_ylabel('Y (meters)')
            ax.grid(True)
            ax.legend(loc='best')
            fig.tight_layout()
            if args.out_dir is not None:
                fig.savefig(f"{args.out_dir}/plot_clusters.png", dpi=150)
                print(f"Saved clusters plot to {args.out_dir}/plot_clusters.png")
            if show:
                plt.show()
        except Exception as e:
            print(f"Failed to generate clusters plot: {e}", file=sys.stderr)


if __name__ == '__main__':
    main()
