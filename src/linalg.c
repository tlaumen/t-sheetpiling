#include "linalg.h"
#include <math.h>
#include <stdlib.h>

int linalg_solve(double *A, double *b, int n)
{
    for (int k = 0; k < n; k++) {
        int piv = k;
        double maxval = fabs(A[(size_t)k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = fabs(A[(size_t)i * n + k]);
            if (v > maxval) { maxval = v; piv = i; }
        }
        if (maxval < 1e-14) return -1;

        if (piv != k) {
            for (int j = 0; j < n; j++) {
                double tmp = A[(size_t)k * n + j];
                A[(size_t)k * n + j] = A[(size_t)piv * n + j];
                A[(size_t)piv * n + j] = tmp;
            }
            double tmp = b[k]; b[k] = b[piv]; b[piv] = tmp;
        }

        for (int i = k + 1; i < n; i++) {
            double factor = A[(size_t)i * n + k] / A[(size_t)k * n + k];
            if (factor == 0.0) continue;
            for (int j = k; j < n; j++)
                A[(size_t)i * n + j] -= factor * A[(size_t)k * n + j];
            b[i] -= factor * b[k];
        }
    }

    for (int i = n - 1; i >= 0; i--) {
        double sum = b[i];
        for (int j = i + 1; j < n; j++)
            sum -= A[(size_t)i * n + j] * b[j];
        b[i] = sum / A[(size_t)i * n + i];
    }
    return 0;
}
