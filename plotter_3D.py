x = [0.457531, 1.12709, -8.94418, -17.9665, -26.8437, -28.4618, -26.2801, -26.7656, -21.1859, -13.062, -5.35646, 3.00743, 6.14319, 5.3453, 4.32981, -0.669558, -6.31616, -7.47115, -12.4426, -15.969, -21.1692, -21.4482, -25.114, -27.2733, -32.0495, -28.5845, -30.5653, -27.2175, -28.2888, -26.9943, -22.2628, -18.8481, -15.5895, -10.9473, -8.18535, -4.64227, -3.35337, 2.72287, 9.2957, 5.93117, 9.98199, 8.76563, 10.9529, 7.49347, 5.1277, 0]
y = [-11.8486, -8.173, -4.60841, -3.93103, -1.82116, -8.26183, -16.6569, -25.0853, -26.7621, -26.6733, -25.9182, -24.6078, -18.4892, -10.5494, -2.59848, -5.10812, -3.32028, -2.36528, -2.1543, -0.421975, -2.73173, -0.632963, -1.26593, -2.67621, -1.97662, -6.69608, -9.06136, -14.5804, -19.4331, -22.6978, -26.9065, -22.4202, -23.1198, -22.5979, -24.7744, -22.7089, -21.3431, -21.0877, -18.045, -13.1257, -11.149, -6.32963, -4.17533, -2.55406, -3.80888, 0]
rssi = [-58, -68, -64, -79, -85, -74, -79, -72, -70, -75, -79, -73, -79, -87, -74, -59, -81, -78, -69, -86, -73, -70, -85, -73, -74, -67, -76, -77, -76, -73, -75, -88, -68, -69, -74, -64, -68, -77, -73, -64, -67, -78, -68, -62, -75, -76]

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
