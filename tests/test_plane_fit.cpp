#include "../src/core/ClusteredTriangulationAlgorithm1.h"

#include <vector>
#include <random>
#include <cmath>
#include <iostream>
#include <gtest/gtest.h>

TEST(PlaneFit, NormalVectorAccuracy)
{
    const double a = 0.5;
    const double b = -0.25;
    const double c = 1.234;
    const int N = 100;
    const double tolerance = 1e-3;

    std::mt19937_64 rng(123456);
    std::uniform_real_distribution<double> unif(-10.0, 10.0);
    std::normal_distribution<double> noise(0.0, 0.01);

    std::vector<double> X(N), Y(N), Z(N);
    for (int i = 0; i < N; ++i)
    {
        double x = unif(rng);
        double y = unif(rng);
        double z = a * x + b * y + c + noise(rng);
        X[i] = x;
        Y[i] = y;
        Z[i] = z;
    }

    auto normal = core::getNormalVector(X, Y, Z);
    ASSERT_EQ(normal.size(), 3u) << "Normal vector size unexpected";

    // Expected normal of plane ax + by - z + c = 0 is [a, b, -1]
    std::vector<double> expected = {a, b, -1.0};

    auto norm = [](const std::vector<double> &v)
    {
        double s = 0.0;
        for (double x : v)
            s += x * x;
        s = std::sqrt(s);
        std::vector<double> r(v.size());
        if (s > 0.0)
            for (size_t i = 0; i < v.size(); ++i)
                r[i] = v[i] / s;
        return r;
    };

    auto n_comp = norm(normal);
    auto n_exp = norm(expected);

    double dot = n_comp[0] * n_exp[0] + n_comp[1] * n_exp[1] + n_comp[2] * n_exp[2];
    double adot = std::abs(dot);

    std::cout << "Computed normal: [" << n_comp[0] << ", " << n_comp[1] << ", " << n_comp[2] << "]\n";
    std::cout << "Expected normal: [" << n_exp[0] << ", " << n_exp[1] << ", " << n_exp[2] << "]\n";
    std::cout << "Abs dot = " << adot << "\n";

    EXPECT_GE(adot, 1.0 - tolerance) << "Normal vector mismatch (abs dot=" << adot << ")";
}