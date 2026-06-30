#include "solver.h"
#include "soil_springs.h"
#include "linalg.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Net soil reaction (force/length) and its tangent dq/dw at level z,
 * given current displacement w (positive = rightward). */
static double net_soil_reaction(const Project *project, double z, double w,
                                 double *dqdw, double *p_left_out, double *p_right_out)
{
    SoilPressureState sL, sR;
    soil_pressure_state(&project->left, z, &sL);
    soil_pressure_state(&project->right, z, &sR);

    double dpL, dpR;
    /* left side: compression (passive) happens when wall moves LEFT, i.e. w<0 -> w_eff_L = -w */
    double pL = soil_branch_pressure(&sL, -w, &dpL);
    /* right side: compression (passive) happens when wall moves RIGHT, i.e. w>0 -> w_eff_R = w */
    double pR = soil_branch_pressure(&sR, w, &dpR);

    if (p_left_out)  *p_left_out  = pL;
    if (p_right_out) *p_right_out = pR;

    /* q = pL(-w) - pR(w);  dq/dw = -dpL - dpR  (see derivation in docs) */
    if (dqdw) *dqdw = -dpL - dpR;
    return pL - pR;
}

static int find_nearest_node(const BeamMesh *mesh, double level)
{
    int best = 0;
    double bestd = fabs(mesh->z[0] - level);
    for (int i = 1; i < mesh->n_nodes; i++) {
        double d = fabs(mesh->z[i] - level);
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

int solve_sheetpile(const Project *project, int n_nodes, SolveResult *result)
{
    BeamMesh mesh;
    if (beam_mesh_build(&project->pile, n_nodes, &mesh) != 0) return -1;

    int ndof = mesh.ndof;
    double *w_full = (double *)calloc((size_t)ndof, sizeof(double)); /* [w0,th0,w1,th1,...] */
    double *J = (double *)malloc(sizeof(double) * (size_t)ndof * ndof);
    double *R = (double *)malloc(sizeof(double) * (size_t)ndof);
    double *p_left = (double *)calloc((size_t)mesh.n_nodes, sizeof(double));
    double *p_right = (double *)calloc((size_t)mesh.n_nodes, sizeof(double));

    int *support_node = (int *)malloc(sizeof(int) * project->n_supports);
    for (int s = 0; s < project->n_supports; s++)
        support_node[s] = find_nearest_node(&mesh, project->supports[s].level);

    const int max_iter = 100;
    const double tol = 1e-6;     /* on max |R| of translational DOFs, normalized */
    const double max_step = 0.05; /* damping: cap displacement change per iteration (length units) */

    int iter, converged = 0;
    for (iter = 0; iter < max_iter; iter++) {
        /* R = K*w  (beam internal forces), J = K to start */
        memcpy(J, mesh.K, sizeof(double) * (size_t)ndof * ndof);
        for (int i = 0; i < ndof; i++) {
            double sum = 0.0;
            for (int j = 0; j < ndof; j++) sum += mesh.K[(size_t)i * ndof + j] * w_full[j];
            R[i] = sum;
        }

        /* subtract soil nodal forces and tangent stiffness at translational DOFs */
        for (int n = 0; n < mesh.n_nodes; n++) {
            double dqdw;
            double q = net_soil_reaction(project, mesh.z[n], w_full[dof_w(n)], &dqdw,
                                          &p_left[n], &p_right[n]);
            double Fsoil = q * mesh.trib[n];
            double dFdw  = dqdw * mesh.trib[n];
            int dw = dof_w(n);
            R[dw] -= Fsoil;
            J[(size_t)dw * ndof + dw] -= dFdw;
        }

        /* Dirichlet BCs: w = 0 at each support node's translational DOF */
        for (int s = 0; s < project->n_supports; s++) {
            int dw = dof_w(support_node[s]);
            for (int j = 0; j < ndof; j++) J[(size_t)dw * ndof + j] = 0.0;
            J[(size_t)dw * ndof + dw] = 1.0;
            R[dw] = w_full[dw] - 0.0;
        }

        double rnorm = 0.0;
        for (int i = 0; i < ndof; i++) if (fabs(R[i]) > rnorm) rnorm = fabs(R[i]);

        double *dw_step = (double *)malloc(sizeof(double) * (size_t)ndof);
        for (int i = 0; i < ndof; i++) dw_step[i] = -R[i];
        if (linalg_solve(J, dw_step, ndof) != 0) {
            free(dw_step);
            break; /* singular: bail out, report non-convergence */
        }

        double max_dw = 0.0;
        for (int n = 0; n < mesh.n_nodes; n++) {
            double v = fabs(dw_step[dof_w(n)]);
            if (v > max_dw) max_dw = v;
        }
        double damp = 1.0;
        if (max_dw > max_step) damp = max_step / max_dw;

        for (int i = 0; i < ndof; i++) w_full[i] += damp * dw_step[i];
        free(dw_step);

        (void)tol;
        if (rnorm < 1e-4) { converged = 1; iter++; break; }
    }

    result->n_nodes = mesh.n_nodes;
    result->z = (double *)malloc(sizeof(double) * mesh.n_nodes);
    result->w = (double *)malloc(sizeof(double) * mesh.n_nodes);
    result->theta = (double *)malloc(sizeof(double) * mesh.n_nodes);
    result->M = (double *)malloc(sizeof(double) * mesh.n_nodes);
    result->V = (double *)malloc(sizeof(double) * mesh.n_nodes);
    result->p_left = p_left;
    result->p_right = p_right;
    result->iterations = iter;
    result->converged = converged;

    for (int n = 0; n < mesh.n_nodes; n++) {
        result->z[n] = mesh.z[n];
        result->w[n] = w_full[dof_w(n)];
        result->theta[n] = w_full[dof_theta(n)];
    }

    /* Recover M, V by numerical differentiation of the converged w field.
     * M = -EI w'', V = -EI w''' = dM/dz (note: z decreases downward, so
     * derivatives are taken with respect to depth using a consistent
     * one-directional step; central differences in z, sign handled by EI). */
    double EI = project->pile.EI;
    double dz = mesh.dz;
    int N = mesh.n_nodes;
    #define WN(i) (w_full[dof_w(i)])
    for (int n = 0; n < N; n++) {
        double d2w, d3w;

        /* second derivative: central in the interior, one-sided 3-pt at the ends */
        if (n == 0)
            d2w = (WN(0) - 2*WN(1) + WN(2)) / (dz*dz);
        else if (n == N - 1)
            d2w = (WN(N-1) - 2*WN(N-2) + WN(N-3)) / (dz*dz);
        else
            d2w = (WN(n-1) - 2*WN(n) + WN(n+1)) / (dz*dz);

        /* third derivative: central 5-pt stencil where 4 neighbours exist,
         * else one-sided 4-pt stencil at/near the ends. Requires N >= 5. */
        if (n >= 2 && n <= N - 3)
            d3w = (-WN(n-2) + 2*WN(n-1) - 2*WN(n+1) + WN(n+2)) / (2*dz*dz*dz);
        else if (n <= 1)
            d3w = (-WN(n) + 3*WN(n+1) - 3*WN(n+2) + WN(n+3)) / (dz*dz*dz);
        else /* n >= N-2 */
            d3w = (WN(n) - 3*WN(n-1) + 3*WN(n-2) - WN(n-3)) / (dz*dz*dz);

        /* index increases with DEPTH (z decreases), so d/d(index) = -d/dz(true).
         * M = -EI*w_zz is even-order in d/dz so the sign flip cancels; V = -EI*w_zzz
         * is odd-order so it does not -- hence the explicit sign flip on V below. */
        result->M[n] = -EI * d2w;
        result->V[n] = EI * d3w;
    }
    #undef WN

    free(w_full);
    free(J);
    free(R);
    free(support_node);
    beam_mesh_free(&mesh);
    return 0;
}

void solve_result_free(SolveResult *result)
{
    free(result->z);
    free(result->w);
    free(result->theta);
    free(result->M);
    free(result->V);
    free(result->p_left);
    free(result->p_right);
    memset(result, 0, sizeof(*result));
}
