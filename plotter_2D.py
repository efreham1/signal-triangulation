x = [-6.31616, -7.47115, -12.4426, -15.969, -21.1692, -21.4482, -25.114, -27.2733, -32.0495, -28.5845, -30.5653, -27.2175, -28.2888, -26.9943, -22.2628, -18.8481, -15.5895, -10.9473, -8.18535, -4.64227, -3.35337, 2.72287, 9.2957, 5.93117, 9.98199, 8.76563, 10.9529, 7.49347, 5.1277, 0]
y = [-3.32028, -2.36528, -2.1543, -0.421975, -2.73173, -0.632963, -1.26593, -2.67621, -1.97662, -6.69608, -9.06136, -14.5804, -19.4331, -22.6978, -26.9065, -22.4202, -23.1198, -22.5979, -24.7744, -22.7089, -21.3431, -21.0877, -18.045, -13.1257, -11.149, -6.32963, -4.17533, -2.55406, -3.80888, 0]
rssi = [-81, -78, -69, -86, -73, -70, -85, -73, -74, -67, -76, -77, -76, -73, -75, -88, -68, -69, -74, -64, -68, -77, -73, -64, -67, -78, -68, -62, -75, -76]
Clusterx = [-8.74331, -19.5288, -28.1456, -28.7891, -25.8487, -15.1283, -5.39366, 5.98324, 9.90016, 4.20706]
Clustery = [-2.61329, -1.26222, -1.97292, -10.1126, -23.0125, -22.7126, -22.9421, -17.4194, -7.218, -2.12098]
AoAs = [150.419, 160.711, -96.6984, 34.1685, -138.776, -84.2136, -50.9914, 96.0582, -9.39557, 51.7035]
def main(save_path: str | None = "plot.png", show: bool = True):
	try:
		import matplotlib.pyplot as plt
		import numpy as np
	except Exception:
		print("matplotlib is required to plot. Install it with: pip install matplotlib")
		raise

	fig, ax = plt.subplots(figsize=(8, 6))
	ax.scatter(x, y, c='C0', label='points')
	ax.plot(x, y, c='C1', linestyle='-', linewidth=1, label='path')
	ax.set_xlabel('x (meters)')
	ax.set_ylabel('y (meters)')
	ax.set_title('Signal measurement points (x, y)')
	ax.grid(True, linestyle='--', alpha=0.5)
	ax.set_aspect('equal', adjustable='box')

	# plot cluster centroids if available
	if 'Clusterx' in globals() and 'Clustery' in globals():
		cx = np.array(Clusterx)
		yc = np.array(Clustery)
		ax.scatter(cx, yc, marker='X', c='C3', s=80, label='centroids')
		# draw AoA arrows if provided
		if 'AoAs' in globals():
			angles = np.array(AoAs)
			# compute a sensible arrow length based on data extent
			xrange = max(x) - min(x) if len(x) > 1 else 1.0
			yrange = max(y) - min(y) if len(y) > 1 else 1.0
			extent = max(xrange, yrange)
			arrow_len = extent * 0.08
			# convert angles (degrees) to dx/dy
			rads = np.deg2rad(angles)
			dxs = arrow_len * np.cos(rads)
			dys = arrow_len * np.sin(rads)
			ax.quiver(cx, yc, dxs, dys, angles='xy', scale_units='xy', scale=1, color='C3', width=0.005)

	ax.legend()

	if save_path:
		fig.savefig(save_path, dpi=200)
		print(f"Saved plot to {save_path}")

	if show:
		plt.show()


if __name__ == '__main__':
	import sys
	save = 'plot.png'
	show = True
	# CLI: --no-show to avoid interactive window, --out <file> to set output filename
	args = sys.argv[1:]
	if '--no-show' in args:
		show = False
	if '--out' in args:
		i = args.index('--out')
		if i + 1 < len(args):
			save = args[i + 1]
	main(save_path=save, show=show)