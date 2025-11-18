#include "tools/plane_test.h"
#include "core/ClusteredTriangulationAlgorithm.h"

#include <vector>
#include <random>
#include <iostream>
#include <cmath>

namespace tools {

bool run_plane_fit_test(double tolerance)
{
    // Construct points lying exactly on plane: z = a*x + b*y + c
    const double a = 0.5;
    const double b = -0.25;
    const double c = 1.234; // constant term

    const int N = 100;
    std::vector<double> xs(N), ys(N), zs(N);

    std::mt19937_64 rng(123456);
    std::uniform_real_distribution<double> unif(-10.0, 10.0);
    std::normal_distribution<double> noise(0.0, 0.01);

    for (int i = 0; i < N; ++i) {
        xs[i] = unif(rng);
        ys[i] = unif(rng);
        zs[i] = a * xs[i] + b * ys[i] + c + noise(rng);
    }


    // Build matrix A with rows=3, cols=N, column-major
    std::vector<double> X(N, 0.0);
    std::vector<double> Y(N, 0.0);
    std::vector<double> Z(N, 0.0);
    for (int j = 0; j < N; ++j) {
        X[j] = xs[j];
        Y[j] = ys[j];
        Z[j] = zs[j];
    }

    auto normal = core::getNormalVector(X, Y, Z);
    if (normal.size() != 3) {
        std::cerr << "getNormalVectorSVD returned unexpected size: " << normal.size() << std::endl;
        return false;
    }

    // Expected normal vector for plane ax + by - z + c = 0 is [a, b, -1]
    std::vector<double> expected = {a, b, -1.0};

    // Normalize both
    auto norm = [](const std::vector<double>& v) {
        double s = 0.0;
        for (double x : v) s += x*x;
        s = std::sqrt(s);
        std::vector<double> r(v.size());
        if (s > 0) for (size_t i = 0; i < v.size(); ++i) r[i] = v[i] / s;
        return r;
    };

    auto nr = norm(normal);
    auto ne = norm(expected);

    double dot = nr[0]*ne[0] + nr[1]*ne[1] + nr[2]*ne[2];
    double adot = std::abs(dot);

    std::cout << "Computed normal: [" << nr[0] << ", " << nr[1] << ", " << nr[2] << "]\n";
    std::cout << "Expected normal (unit): [" << ne[0] << ", " << ne[1] << ", " << ne[2] << "]\n";
    std::cout << "Absolute dot product = " << adot << " (1.0 = perfect match, up to sign)\n";

    if (adot >= (1.0 - tolerance)) {
        std::cout << "Plane-fit SVD normal OK (within tolerance " << tolerance << ")\n";
        return true;
    } else {
        std::cerr << "Plane-fit SVD normal FAILED (adot=" << adot << ")\n";
        return false;
    }
}

} // namespace tools
