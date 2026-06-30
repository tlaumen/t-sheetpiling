#include "model.h"
#include "solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(void)
{
    /* --- right side: retained ground, full overburden from level 0.0 --- */
    SoilLayer right_layers[1] = {
        { .top = 0.0, .bottom = -15.0, .gamma = 18.0, .phi_deg = 30.0,
          .cohesion = 0.0, .k1 = 15000.0, .k2 = 6000.0, .k3 = 2000.0 }
    };
    SoilProfile right = { .layers = right_layers, .n_layers = 1, .top_level = 0.0 };

    /* --- left side: excavated down to -3.0, same soil below that --- */
    SoilLayer left_layers[1] = {
        { .top = -3.0, .bottom = -15.0, .gamma = 18.0, .phi_deg = 30.0,
          .cohesion = 0.0, .k1 = 15000.0, .k2 = 6000.0, .k3 = 2000.0 }
    };
    SoilProfile left = { .layers = left_layers, .n_layers = 1, .top_level = -3.0 };

    SheetPile pile = { .EI = 100000.0, .top_level = 0.5, .tip_level = -10.0 };

    Support supports[1] = {
        { .level = -1.0, .type = SUPPORT_FIXED_HORIZONTAL }
    };

    Project project = {
        .pile = pile, .left = left, .right = right,
        .supports = supports, .n_supports = 1
    };

    SolveResult result;
    int rc = solve_sheetpile(&project, 61, &result);
    if (rc != 0) { fprintf(stderr, "solve failed (rc=%d)\n", rc); return 1; }

    printf("converged=%d iterations=%d\n\n", result.converged, result.iterations);
    printf("%8s %10s %10s %10s %10s %12s %12s\n",
           "z", "w[mm]", "theta", "M[kNm]", "V[kN]", "p_left", "p_right");
    for (int i = 0; i < result.n_nodes; i++) {
        printf("%8.2f %10.3f %10.6f %10.2f %10.2f %12.2f %12.2f\n",
               result.z[i], result.w[i] * 1000.0, result.theta[i],
               result.M[i], result.V[i], result.p_left[i], result.p_right[i]);
    }

    /* find max moment for a quick sanity summary */
    double Mmax = 0.0, zMmax = 0.0;
    for (int i = 0; i < result.n_nodes; i++) {
        if (fabs(result.M[i]) > fabs(Mmax)) { Mmax = result.M[i]; zMmax = result.z[i]; }
    }
    printf("\nMax |M| = %.2f kNm at z = %.2f\n", Mmax, zMmax);

    /* dump CSV for plotting */
    FILE *f = fopen("/home/claude/sheetpile/bin/result.csv", "w");
    if (f) {
        fprintf(f, "z,w_mm,theta,M_kNm,V_kN,p_left,p_right\n");
        for (int i = 0; i < result.n_nodes; i++)
            fprintf(f, "%.4f,%.4f,%.6f,%.4f,%.4f,%.4f,%.4f\n",
                    result.z[i], result.w[i] * 1000.0, result.theta[i],
                    result.M[i], result.V[i], result.p_left[i], result.p_right[i]);
        fclose(f);
    }

    solve_result_free(&result);
    return 0;
}
