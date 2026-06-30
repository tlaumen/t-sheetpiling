#include "model.h"
#include "solver.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Flat C API for FFI callers (e.g. Python ctypes). All arrays are
 * caller-allocated; this function only reads inputs and writes outputs,
 * it never allocates anything the caller has to free.
 *
 * Layer arrays (left/right) use struct-of-arrays layout, length n_left /
 * n_right. Output arrays must be sized >= n_nodes (the n_nodes you pass
 * in as the mesh resolution, which equals the number of result rows).
 *
 * Returns 0 on success, -1 on bad input, -2 if the mesh could not be built.
 */
int solve_api(
    double pile_EI, double pile_top, double pile_tip,
    const double *left_top, const double *left_bottom, const double *left_gamma,
    const double *left_phi, const double *left_c,
    const double *left_k1, const double *left_k2, const double *left_k3,
    int n_left,
    const double *right_top, const double *right_bottom, const double *right_gamma,
    const double *right_phi, const double *right_c,
    const double *right_k1, const double *right_k2, const double *right_k3,
    int n_right,
    const double *support_levels, int n_supports,
    int n_nodes,
    double *out_z, double *out_w, double *out_theta, double *out_M, double *out_V,
    double *out_p_left, double *out_p_right,
    int *out_converged, int *out_iterations)
{
    if (n_left <= 0 || n_right <= 0 || n_nodes < 5) return -1;

    SoilLayer *left_layers = (SoilLayer *)malloc(sizeof(SoilLayer) * (size_t)n_left);
    SoilLayer *right_layers = (SoilLayer *)malloc(sizeof(SoilLayer) * (size_t)n_right);
    Support *supports = (Support *)malloc(sizeof(Support) * (size_t)(n_supports > 0 ? n_supports : 1));
    if (!left_layers || !right_layers || !supports) {
        free(left_layers); free(right_layers); free(supports);
        return -1;
    }

    for (int i = 0; i < n_left; i++) {
        left_layers[i] = (SoilLayer){
            .top = left_top[i], .bottom = left_bottom[i], .gamma = left_gamma[i],
            .phi_deg = left_phi[i], .cohesion = left_c[i],
            .k1 = left_k1[i], .k2 = left_k2[i], .k3 = left_k3[i]
        };
    }
    for (int i = 0; i < n_right; i++) {
        right_layers[i] = (SoilLayer){
            .top = right_top[i], .bottom = right_bottom[i], .gamma = right_gamma[i],
            .phi_deg = right_phi[i], .cohesion = right_c[i],
            .k1 = right_k1[i], .k2 = right_k2[i], .k3 = right_k3[i]
        };
    }
    for (int i = 0; i < n_supports; i++) {
        supports[i] = (Support){ .level = support_levels[i], .type = SUPPORT_FIXED_HORIZONTAL };
    }

    SoilProfile left = { .layers = left_layers, .n_layers = n_left, .top_level = left_layers[0].top };
    SoilProfile right = { .layers = right_layers, .n_layers = n_right, .top_level = right_layers[0].top };
    SheetPile pile = { .EI = pile_EI, .top_level = pile_top, .tip_level = pile_tip };
    Project project = { .pile = pile, .left = left, .right = right,
                         .supports = supports, .n_supports = n_supports };

    SolveResult result;
    int rc = solve_sheetpile(&project, n_nodes, &result);

    free(left_layers);
    free(right_layers);
    free(supports);

    if (rc != 0) return -2;

    for (int i = 0; i < result.n_nodes; i++) {
        out_z[i] = result.z[i];
        out_w[i] = result.w[i];
        out_theta[i] = result.theta[i];
        out_M[i] = result.M[i];
        out_V[i] = result.V[i];
        out_p_left[i] = result.p_left[i];
        out_p_right[i] = result.p_right[i];
    }
    *out_converged = result.converged;
    *out_iterations = result.iterations;

    solve_result_free(&result);
    return 0;
}
