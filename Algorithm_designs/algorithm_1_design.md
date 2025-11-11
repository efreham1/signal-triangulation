# Algorithm 1 — Cluster-based Angle-of-Arrival (AoA) Triangulation

## Summary

Pros:

- May produce more accurate localization than simpler methods in complex environments where signal variation is nontrivial.

Cons:

- More complex to implement.
- Higher computational cost compared to simpler heuristics.

## Objective

Given a set of timestamped measurements with GPS coordinates and signal strength values, the algorithm groups nearby measurements into clusters, estimates a local angle of arrival (AoA) and average strength for each cluster, converts each cluster into a vector estimate, and then estimates the source location by minimizing a global cost function over those vectors.

## High-level steps

1. Partition the measured datapoints into clusters.
2. For each cluster, estimate the Angle of Arrival (AoA) and the cluster's average signal strength.
3. Convert each cluster into a vector: direction = AoA, magnitude = average strength.
4. Define a cost function that quantifies the mismatch between a candidate source location and the set of cluster vectors.
5. Detect and exclude outlier clusters.
6. Minimize the cost function (e.g., gradient descent or a robust optimizer) to obtain the estimated source location.

## Clustering of datapoints

Notes and requirements:

- Clusters should contain enough points to produce a stable AoA estimate, but should not be so large that the true AoA varies substantially within a cluster.
- Overlapping clusters may be acceptable when measurements are dense or when temporal grouping is used.
- Clustering criteria should consider both geographic proximity (GPS) and signal-strength similarity.
- The clustering strategy should be chosen with awareness of the AoA estimation method used subsequently.

Simple initial clustering heuristic (baseline):

- Partition the data into sequential groups of three measurements (p0..p2, p3..p5, ...). This assumes data were collected in order and that adjacent measurements are geographically proximate. This simple heuristic does not address all edge cases and should be replaced by a spatial clustering algorithm (e.g., DBSCAN or agglomerative clustering) in production.

## Estimating Angle of Arrival (AoA)

Model and intuition:

- Treat signal strength s as a smooth scalar field over the plane defined by GPS coordinates (x, y). Fit a local plane z = a x + b y + c to the cluster, where z corresponds to measured signal strength.
- The gradient (a, b) of the fitted plane indicates the direction of greatest increase in signal strength; i.e., the projected direction toward the source in the local neighborhood. The AoA direction may be taken as the normalized vector (a, b).

Given three non-collinear points in a cluster, a, b, c can be solved directly by linear algebra. Let the points be (x1, y1, s1), (x2, y2, s2), (x3, y3, s3). The corresponding linear system may be written explicitly as:

$$
\begin{cases}
s_1 = a x_1 + b y_1 + c \\
s_2 = a x_2 + b y_2 + c \\
s_3 = a x_3 + b y_3 + c
\end{cases}
$$

Solve for a, b, c using a numerically stable method (e.g., QR decomposition or a small linear solve routine). The local AoA vector is then v = (a, b).

Practical considerations:

- If clusters contain more than three points, perform a least-squares fit to improve robustness to measurement noise.
- Pre-filter or weight points within the cluster by measurement quality, e.g., remove points with invalid GPS or anomalous strength readings.

## Creating cluster vectors

Procedure:

1. Compute the local gradient v = (a, b) from the plane fit.
2. Normalize v to obtain the direction: u = v / ||v||.
3. Compute the cluster's representative magnitude, for example the arithmetic mean of the signal strengths in the cluster: m = mean(s_i).
4. Define the cluster vector as w = m * u.

Rationale: the direction encodes the estimated bearing toward the source, while the magnitude encodes confidence or relative influence. Alternative scaling strategies (e.g., using inverse noise variance) may be used when measurement variances are known.

## Signal source localization

Cost function design:

- Represent each cluster by a line (or ray) passing through the cluster centroid in direction u. For a candidate source location x, compute the perpendicular distance from x to the line defined by the cluster vector; denote this distance d_i(x).
- A simple global cost is the sum of squared perpendicular distances, possibly with weights:

$$
C(x) = \sum_i w_i \cdot d_i(x)^2
$$

where w_i is a weight for cluster i (e.g., proportional to m or to a confidence metric). Note: weighting by raw signal strength may bias the solution toward nearby strong measurements; choose weights carefully or use a robust weighting scheme.

Outlier detection and robustness:

- Identify clusters with residuals that are significantly larger than the median or mean residual. Remove or downweight those clusters (robust loss functions such as Huber or Tukey loss may be preferable to hard removal).
- Consider signal-strength-based heuristics: clusters with anomalously low or high strengths relative to nearby clusters might indicate multipath or measurement error.

Optimization:

- Minimize C(x) with respect to x using gradient descent starting at multiple initial points to avoid local minima.
- Initialize the optimizer using the intersections of two cluster rays. Next inital point should be chosen from a different pair of cluster rays to ensure diverse starting locations.

## Edge cases and considerations
- Collinear or nearly collinear GPS samples produce ill-conditioned plane fits; detect and handle via regularization or re-clustering.
- Multipath propagation can produce misleading local gradients; robust weighting and outlier rejection mitigate this.
- GPS inaccuracy