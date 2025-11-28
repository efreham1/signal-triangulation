#include "../src/core/ClusteredTriangulationBase.h"

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

    // Use the free function instead of static member
    auto normal = core::fitPlaneNormal(X, Y, Z);
    ASSERT_EQ(normal.size(), 3u) << "Normal vector size unexpected";

    // Expected normal of plane z = ax + by + c is [a, b, -1]
    std::vector<double> expected = {a, b, -1.0};

    auto normalize = [](const std::vector<double> &v)
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

    auto n_comp = normalize(normal);
    auto n_exp = normalize(expected);

    // Vectors can point in opposite directions, so check |dot| ≈ 1
    double dot = n_comp[0] * n_exp[0] + n_comp[1] * n_exp[1] + n_comp[2] * n_exp[2];
    double adot = std::abs(dot);

    std::cout << "Computed normal: [" << n_comp[0] << ", " << n_comp[1] << ", " << n_comp[2] << "]\n";
    std::cout << "Expected normal: [" << n_exp[0] << ", " << n_exp[1] << ", " << n_exp[2] << "]\n";
    std::cout << "Abs dot = " << adot << "\n";

    EXPECT_GE(adot, 1.0 - tolerance) << "Normal vector mismatch (abs dot=" << adot << ")";
}

TEST(PlaneFit, MinimumPoints)
{
    // Test with exactly 3 points (minimum for plane fitting)
    std::vector<double> X = {0.0, 1.0, 0.0};
    std::vector<double> Y = {0.0, 0.0, 1.0};
    std::vector<double> Z = {0.0, 1.0, 2.0}; // z = x + 2y

    auto normal = core::fitPlaneNormal(X, Y, Z);
    ASSERT_EQ(normal.size(), 3u);

    // Expected normal: [1, 2, -1]
    std::vector<double> expected = {1.0, 2.0, -1.0};

    auto normalize = [](const std::vector<double> &v)
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

    auto n_comp = normalize(normal);
    auto n_exp = normalize(expected);

    double dot = n_comp[0] * n_exp[0] + n_comp[1] * n_exp[1] + n_comp[2] * n_exp[2];
    EXPECT_GE(std::abs(dot), 0.99) << "Normal vector mismatch for minimum points";
}

TEST(PlaneFit, HorizontalPlane)
{
    // Test a horizontal plane (z = constant)
    std::vector<double> X = {0.0, 1.0, 2.0, 0.0, 1.0};
    std::vector<double> Y = {0.0, 0.0, 0.0, 1.0, 1.0};
    std::vector<double> Z = {5.0, 5.0, 5.0, 5.0, 5.0};

    auto normal = core::fitPlaneNormal(X, Y, Z);
    ASSERT_EQ(normal.size(), 3u);

    // Expected normal: [0, 0, -1] (or [0, 0, 1])
    // The x and y components should be near zero
    EXPECT_NEAR(normal[0], 0.0, 0.01) << "X component should be ~0 for horizontal plane";
    EXPECT_NEAR(normal[1], 0.0, 0.01) << "Y component should be ~0 for horizontal plane";
    EXPECT_NE(normal[2], 0.0) << "Z component should be non-zero for horizontal plane";
}