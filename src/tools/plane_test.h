#ifndef PLANE_TEST_H
#define PLANE_TEST_H

namespace tools {
    // Run a plane-fit normal-vector test. Returns true on success.
    bool run_plane_fit_test(double tolerance = 1e-3);
}

#endif // PLANE_TEST_H
