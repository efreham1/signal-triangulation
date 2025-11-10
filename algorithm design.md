Algorithm 1:
    Pro:
        Probably better results than Algorithm 2 in complex environments
    Con:
        More complex to implement
        More computationally expensive

    steps:
        cluster datapoints
        for each cluster calculate the likely angle of arrival and the average signal strength
        create vectors from each cluster, angle is angle of arrival, magnitude is signal strength
        outlier detenction?
        find a location for a signal source that minimizes a cost function that is based on the vectors

    Clustering of datapoints:
        Notes:
            Has to have enough clustering to get a reliable AoA but not so much that the true AoA varies greatly between points
            Should result in Overlapping clustering with outliers
            Should be based on both signal strength and gps coordinate
            The clusterting here should probably be dependent on the AoA algorithm used later

    Calculating AoA:
        Fit a plane z = ax + by + c to the cluster points where z is signal strength and (x,y) are gps coordinates
        The gradient of the plane gives the direction of maximum increase in signal strength
        

    Creating vectors:
        Trivial

    Signal source location:
        Cost function:
            For each cluster vector calculate the perpendicular distance from the signal source to the line defined by the vector
            Sum all distances to get total cost.
            I belive weighting by signal strength might be a bad idea since stronger signals might be closer and thus have a larger angle error
        Outlier detection:
            If a cluster's distance is significantly higher than the average distance, consider it an outlier and exclude it from the cost function.
            Maybe also consider signal strength in outlier detection? Like if a cluster has way lower or way higher signal strength than the average?

        Minimize cost function using gradient descent or similar


Algorithm 2 (GPT-ad) https://chatgpt.com/s/t_6911ed36698c81918ec55784fe2eb762:
    Con:
        The fit will most likely suck ass since it will try to fit an ideal curve to a very non-ideal reality
    Notation:
        i = 1..N indexes the N measurements.
        (x_i, y_i) are known coordinates of measurement i.
        P_i is the measured RSS (in dBm) at (x_i, y_i).
        Unknown parameters to estimate: x, y (AP position). Optionally P0 (reference RSS at 1 m) and n (path-loss exponent).
        d_i = distance from AP to measurement i = sqrt((x - x_i)^2 + (y - y_i)^2).
        P_model(d) = P0 - 10 * n * log10(d). (This is the standard log-distance path loss model.)
        ε_i is measurement noise (assumed zero-mean).

    Model equations (per measurement)
    P_i = P_model(d_i) + ε_i
    P_i = P0 - 10 * n * log10(d_i) + ε_i

    Objective function (nonlinear least squares)
    Find the parameters that minimize the sum of squared residuals:

    Minimize over parameters θ = (x, y [, P0 [, n]]) the cost:
    C(θ) = sum_{i=1..N} r_i(θ)^2

    where the residual for sample i is:
    r_i(θ) = P_model(d_i; P0, n) - P_i
    = P0 - 10 * n * log10(d_i) - P_i

    Notes:
    • If you choose to fix P0 and/or n (e.g., known or assumed), remove them from θ and use those fixed values in P_model.
    • To avoid log10(0), clamp d_i to a small positive lower bound (e.g., d_i = max(d_i, 1e-6)).

    Robustness improvements (optional)
    • Use a robust loss instead of plain squared error (Huber, soft L1) to reduce sensitivity to outliers: minimize sum rho(r_i) rather than r_i^2.
    • Use RANSAC: repeatedly fit on random subsets, keep the fit with most inliers, then refine.

    Initial guess (important to avoid bad local minima)
    • Weighted centroid initial guess for position:
    weights w_i = 10^(P_i / 10) (linear power)
    x_init = sum(w_i * x_i) / sum(w_i)
    y_init = sum(w_i * y_i) / sum(w_i)
    • For P0 init: use max measured P_i or average of top k P_i.
    • For n init: use a typical value like 2.0 or 3.0, or estimate from data if you have range diversity.

    Algorithmic implementation (high-level)

    Input: coordinates (x_i, y_i), RSS values P_i, optionally fixed P0 or n.

    Build the residual function r(θ) that returns vector [r_1, ..., r_N] for given θ.

    Choose θ_init from the initial-guess recipe above.

    Use a nonlinear least-squares optimizer (e.g., Levenberg–Marquardt or trust-region) to minimize sum of squared residuals:
    θ_hat = argmin_theta sum r_i(θ)^2
    If using a library function, supply r(θ) (and optionally its Jacobian) and θ_init.

    If desired, repeat with robust loss or RANSAC to reject outliers, then refine with least squares.

    Jacobian (optional, useful for LM or faster convergence)
    If θ = (x, y, P0, n), compute partial derivatives of r_i with respect to each parameter:

    Let d_i = sqrt((x-x_i)^2 + (y-y_i)^2).
    Let L_i = log10(d_i). (log base 10)
    Then
    r_i = P0 - 10 * n * L_i - P_i

    Partial derivatives:
    ∂r_i/∂P0 = 1
    ∂r_i/∂n = -10 * L_i
    ∂r_i/∂x = -10 * n * (1 / (ln(10) * d_i)) * ( (x - x_i) / d_i )
    = -10 * n * (x - x_i) / (ln(10) * d_i^2)
    ∂r_i/∂y = -10 * n * (y - y_i) / (ln(10) * d_i^2)

    Explanation of derivatives for ∂r_i/∂x and ∂r_i/∂y:
    • d/dx log10(d_i) = (1 / (ln(10) * d_i)) * d_i/dx = (x - x_i) / (ln(10) * d_i^2).

    If P0 or n are fixed, omit the corresponding Jacobian columns.

    Bounds and regularization (practical)
    • Optionally bound x,y to a search area (box) if you know the region where AP can be. Many optimizers support bounds.
    • If jointly estimating n, regularize n toward a prior (e.g., add λ*(n - n_prior)^2 to cost) to avoid pathological fits when data is insufficient.

    Grid-search + refinement (robust initialization)
    • For robustness, you can evaluate C(x,y) on a coarse grid over the plausible area (using fixed n and P0 or plugging in best local P0) and pick the grid point with minimum cost as θ_init, then run nonlinear refinement.

    Output and uncertainty
    • Optimizer will give best-fit θ_hat. If the optimizer provides a covariance estimate (approx inverse Hessian), you can use it to get uncertainty on position. Note that model mismatch (multipath, non-log-distance behavior) makes uncertainty optimistic.

    When to estimate P0 and n jointly vs. fix them
    • If you have many measurements at various distances spanning a large range, estimating P0 and n jointly is feasible and better.
    • If you only have a small cluster of points or limited distance diversity, fix n to a plausible value and/or calibrate P0 externally.

    Practical caveats
    • RSS variance, multipath, shadowing, and device antenna gains often violate the simple model. Use robust loss, outlier rejection, or GP-based residual modeling if errors are structured.
    • Geometry matters: spatially well-distributed samples around the AP produce better localization than samples all on one side.