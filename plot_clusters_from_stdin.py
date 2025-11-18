#!/usr/bin/env python3
"""
Read cluster blocks from stdin and plot clusters in 2D with centroids and AoA arrows.

Expected input format (produced by the C++ code):

cluster:<id> points:<n> centroid_x:<cx> centroid_y:<cy> avg_rssi:<avg> aoa_x:<ax> aoa_y:<ay> estimated_aoa:<deg> furthest:<i>,<j> furthest_distance:<d>
p <x> <y> <rssi> <timestamp>
p <x> <y> <rssi> <timestamp>

(blank line separates clusters)

Usage:
  ./build/signal-triangulation <json-file> | python3 plot_clusters_from_stdin.py

Optional flags: -o <file> to save instead of show.
"""
import sys
import math
import argparse
from collections import namedtuple

import matplotlib.pyplot as plt

Cluster = namedtuple('Cluster', ['id','points','centroid_x','centroid_y','ratio'])


def parse_input(lines):
    """Parse the compact cluster format emitted by the C++ tool.

    Expected tokens:
      cluster:<id> centroid_x:<cx> centroid_y:<cy> ratio:<r>
      p <x> <y>

    Blank line separates clusters.
    """
    clusters = []
    cur_cluster = None
    cur_points = []
    for raw in lines:
        line = raw.strip()
        if not line:
            if cur_cluster is not None:
                clusters.append(Cluster(cur_cluster.get('id', -1), cur_points, cur_cluster.get('centroid_x', 0.0), cur_cluster.get('centroid_y', 0.0), cur_cluster.get('ratio', 0.0)))
                cur_cluster = None
                cur_points = []
            continue

        if line.startswith('cluster:'):
            tokens = line.split()
            data = {}
            # cluster:<id>
            try:
                first = tokens[0]
                _, cid = first.split(':', 1)
                data['id'] = int(cid)
            except Exception:
                data['id'] = -1
            for tok in tokens[1:]:
                if ':' not in tok:
                    continue
                k, v = tok.split(':', 1)
                if k in ('centroid_x', 'centroid_y', 'ratio'):
                    try:
                        data[k] = float(v)
                    except Exception:
                        data[k] = 0.0
            cur_cluster = data
        elif line.startswith('p '):
            parts = line.split()
            if len(parts) >= 3:
                try:
                    x = float(parts[1])
                    y = float(parts[2])
                except Exception:
                    continue
                cur_points.append((x, y))

    # finalize last
    if cur_cluster is not None:
        clusters.append(Cluster(cur_cluster.get('id', -1), cur_points, cur_cluster.get('centroid_x', 0.0), cur_cluster.get('centroid_y', 0.0), cur_cluster.get('ratio', 0.0)))
    return clusters


def plot_clusters(clusters, save_path=None):
    if not clusters:
        print('No clusters parsed from stdin', file=sys.stderr)
        return
    fig, ax = plt.subplots(figsize=(8,8))
    colors = plt.get_cmap('tab10')

    # Find overall extents to scale arrows
    xs = []
    ys = []
    for c in clusters:
        for p in c.points:
            xs.append(p[0]); ys.append(p[1])
    if not xs or not ys:
        print('No point coordinates found', file=sys.stderr)
        return
    span = max(max(xs)-min(xs), max(ys)-min(ys))
    if span == 0:
        span = 1.0
    arrow_scale = span * 0.2

    for idx, c in enumerate(clusters):
        pts = c.points
        cx = c.centroid_x
        cy = c.centroid_y
        color = colors(idx % 10)
        if pts:
            px = [p[0] for p in pts]
            py = [p[1] for p in pts]
            ax.scatter(px, py, color=color, label=f'cluster {c.id} pts', alpha=0.7)
        # centroid (marked and labeled with ratio)
        ax.scatter([cx], [cy], marker='x', color='k')
        ax.text(cx, cy, f'c{c.id} r={c.ratio:.2f}', fontsize=9, color='k')

    ax.set_xlabel('X (meters)')
    ax.set_ylabel('Y (meters)')
    ax.set_aspect('equal', adjustable='box')
    ax.grid(True)
    ax.legend(loc='best')
    plt.tight_layout()
    if save_path:
        plt.savefig(save_path, dpi=150)
        print(f'Saved plot to {save_path}')
    else:
        plt.show()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-o','--output', help='Save plot to file instead of showing', default=None)
    args = parser.parse_args()

    data = sys.stdin.read().splitlines()
    clusters = parse_input(data)
    plot_clusters(clusters, save_path=args.output)
