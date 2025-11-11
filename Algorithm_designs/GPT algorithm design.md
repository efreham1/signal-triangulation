
# Algorithm 2 (GPT-ad)
Link: https://chatgpt.com/s/t_6911ed36698c81918ec55784fe2eb762


$$
\hat{\theta} = \operatorname{argmin}_{\theta} \sum_{i} r_i(\theta)^2
$$


$$
d_i = \sqrt{(x - x_i)^2 + (y - y_i)^2}.
$$


$$
P_{\mathrm{model}}(d) = P_0 - 10 n \log_{10}(d).
$$


## Model equations (per measurement)

$$
P_i = P_{\mathrm{model}}(d_i) + \varepsilon_i
= P_0 - 10 n \log_{10}(d_i) + \varepsilon_i.
$$

## Objective function (nonlinear least squares)

Find parameters $\theta = (x, y[, P_0[, n]])$ that minimize the sum of squared residuals:

$$
C(\theta) = \sum_{i=1}^N r_i(\theta)^2,
$$

where the residual for sample $i$ is

$$
r_i(\theta) = P_{\mathrm{model}}(d_i; P_0, n) - P_i
= P_0 - 10 n \log_{10}(d_i) - P_i.
$$

Notes:


## Robustness improvements (optional)


## Initial guess (important to avoid bad local minima)


$$
w_i = 10^{P_i / 10} \quad (\text{linear power})
$$

$$
x_{\mathrm{init}} = \frac{\sum_i w_i x_i}{\sum_i w_i}, \quad
y_{\mathrm{init}} = \frac{\sum_i w_i y_i}{\sum_i w_i}.
$$


## Algorithmic implementation (high-level)

Input: coordinates $(x_i, y_i)$, RSS values $P_i$, optionally fixed $P_0$ or $n$.

1. Build the residual function $r(\theta)$ that returns $[r_1,\dots,r_N]$ for given $\theta$.
2. Choose $\theta_{\mathrm{init}}$ from the initial-guess recipe above.
3. Use a nonlinear least-squares optimizer (e.g., Levenberg–Marquardt or trust-region) to minimize $\sum r_i(\theta)^2$:

$$
\hat{\theta} = \operatorname{argmin}_{\theta} \sum_{i} r_i(\theta)^2
$$

If using a library, supply $r(\theta)$ (and optionally its Jacobian) and $\theta_{\mathrm{init}}$.

If desired, repeat with a robust loss or RANSAC to reject outliers, then refine with least squares.

## Jacobian (optional, useful for LM or faster convergence)

Assume $\theta = (x, y, P_0, n)$. Define

$$
d_i = \sqrt{(x-x_i)^2 + (y-y_i)^2}, \quad L_i = \log_{10}(d_i),
\quad r_i = P_0 - 10 n L_i - P_i.
$$

Partial derivatives:

$$
\frac{\partial r_i}{\partial P_0} = 1,
\qquad
\frac{\partial r_i}{\partial n} = -10 L_i,
$$

and for the position components (using $\mathrm{ln}$ for natural log):

$$
\frac{\partial r_i}{\partial x} = -10 n \frac{1}{\ln 10} \cdot \frac{x - x_i}{d_i^2},
\qquad
\frac{\partial r_i}{\partial y} = -10 n \frac{1}{\ln 10} \cdot \frac{y - y_i}{d_i^2}.
$$

Explanation:

$$
\frac{d}{dx} \log_{10}(d_i) = \frac{1}{\ln 10} \cdot \frac{1}{d_i} \cdot \frac{\partial d_i}{\partial x}
= \frac{x - x_i}{\ln 10 \; d_i^2}.
$$

If $P_0$ or $n$ are fixed, omit the corresponding Jacobian columns.

- Optionally bound $x,y$ to a search area (box) if you know the plausible region; many optimizers support bounds.
- If jointly estimating $n$, regularize $n$ toward a prior (e.g., add $\lambda (n - n_{\mathrm{prior}})^2$ to the cost) to avoid pathological fits when data are insufficient.


- For robustness, evaluate $C(x,y)$ on a coarse grid over the plausible area (using fixed $n$ and $P_0$ or plugging in a local best $P_0$). Pick the grid point with minimum cost as $\theta_{\mathrm{init}}$, then run nonlinear refinement.


- The optimizer returns best-fit $\hat{\theta}$. If the optimizer provides a covariance estimate (approximate inverse Hessian), use it to obtain uncertainty on position. Model mismatch (multipath, non-log-distance behavior) will make these uncertainties optimistic.

- If measurements span a wide range of distances, estimating $P_0$ and $n$ jointly is feasible and often preferable.
- If data are limited in distance diversity, fix $n$ to a plausible value and/or calibrate $P_0$ externally.

- RSS variance, multipath, shadowing, and device antenna gains often violate the simple model. Use robust loss, outlier rejection, or GP-based residual modeling if errors exhibit structure.
- Geometry matters: spatially well-distributed samples around the AP produce better localization than samples confined to one side.