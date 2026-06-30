#ifndef LINALG_H
#define LINALG_H

/* Solves A x = b for a dense n*n system (row-major A). A and b are
 * overwritten as scratch space. Returns 0 on success, -1 if singular. */
int linalg_solve(double *A, double *b, int n);

#endif
