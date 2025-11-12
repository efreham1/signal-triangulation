x = [50.349, 50.2764, 44.6028, 36.4577, 26.3098, 20.4297, 11.4143, 1.97491, 3.94423, 8.32362, 15.9053, 21.0322, 34.6669, 38.9179, 44.8036, 55.4982, 59.8553, 59.8497, 61.8637, 68.7591, 72.1287, 79.7885, 84.5249, 90.5947, 98.7342, 95.9726, 90.5277, 80.7759, 71.4425, 65.4676, 56.0952, 50.6558, 51.7437, 49.4061, 43.6823, 36.0616, 31.9723, 25.8244, 18.929, 0]
y = [21.6096, 21.654, 18.2671, 17.0789, 16.4015, 15.2911, 15.8241, 8.4284, -8.03974, -11.4711, -18.7779, -23.7528, -30.1268, -25.7294, -19.533, -6.71829, -4.3308, -6.34073, -5.82992, -6.88486, -8.36177, -7.54003, 0.366452, 2.79836, 6.61835, 13.4033, 16.024, 13.5587, 11.6709, 10.6493, 8.4284, 9.1391, 7.75102, 6.16306, 7.88428, 6.36294, 6.14085, 3.8533, 1.17709, 0]
rssi = [-81, -67, -70, -79, -76, -78, -75, -94, -93, -92, -89, -81, -87, -87, -81, -70, -67, -68, -79, -70, -76, -73, -86, -94, -86, -83, -72, -67, -64, -63, -63, -47, -51, -49, -67, -75, -64, -76, -76, -86]

def main(save_path: str | None = "plot_3d.png", show: bool = True, cmap: str = 'viridis'):
	try:
		import matplotlib.pyplot as plt
		from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
		import numpy as np
	except Exception:
		print("matplotlib and numpy are required to plot. Install with: pip install matplotlib numpy")
		raise

	if not (len(x) == len(y) == len(rssi)):
		raise ValueError("x, y and rssi lists must have the same length")

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

	if save_path:
		fig.savefig(save_path, dpi=200)
		print(f"Saved 3D plot to {save_path}")

	if show:
		plt.show()


if __name__ == '__main__':
	import sys
	save = 'plot_3d.png'
	show = True
	cmap = 'viridis'
	args = sys.argv[1:]
	if '--no-show' in args:
		show = False
	if '--out' in args:
		i = args.index('--out')
		if i + 1 < len(args):
			save = args[i + 1]
	if '--cmap' in args:
		i = args.index('--cmap')
		if i + 1 < len(args):
			cmap = args[i + 1]
	main(save_path=save, show=show, cmap=cmap)
