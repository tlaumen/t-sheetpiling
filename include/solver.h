#ifndef SOLVER_H
#define SOLVER_H

#include "model.h"
#include "beam_fe.h"

typedef struct {
    int n_nodes;
    double *z;        /* node levels */
    double *w;        /* horizontal displacement */
    double *theta;     /* rotation */
    double *M;         /* bending moment, M = -EI * d2w/dz2 */
    double *V;         /* shear force,   V = -EI * d3w/dz3 */
    double *p_left;     /* converged soil pressure, left side, per node */
    double *p_right;    /* converged soil pressure, right side, per node */
    int iterations;
    int converged;
} SolveResult;

/* Solves the full nonlinear problem (tri-linear active/neutral/passive
 * soil springs on both sides, elastic beam, fixed-horizontal supports)
 * via damped Newton-Raphson. n_nodes controls mesh resolution.
 * Returns 0 on success. Caller must call solve_result_free when done. */
int solve_sheetpile(const Project *project, int n_nodes,
                     SolveResult *result);
void solve_result_free(SolveResult *result);

#endif
