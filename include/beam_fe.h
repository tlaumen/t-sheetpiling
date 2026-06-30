#ifndef BEAM_FE_H
#define BEAM_FE_H

#include "model.h"

/* A uniform finite-element mesh of the sheet pile. DOF layout per node i
 * is [w_i, theta_i] -> global indices [2*i, 2*i+1]. ndof = 2*n_nodes. */
typedef struct {
    int n_nodes;
    int ndof;
    double dz;
    double *z;        /* node levels, length n_nodes, z[0]=top_level (descending) */
    double *trib;      /* tributary length per node for lumping distributed soil load */
    double *K;        /* dense global elastic stiffness matrix, ndof*ndof, row-major */
} BeamMesh;

/* Builds the mesh and assembles the constant elastic beam stiffness matrix.
 * n_nodes must be >= 2. Caller must call beam_mesh_free when done. */
int beam_mesh_build(const SheetPile *pile, int n_nodes, BeamMesh *mesh);
void beam_mesh_free(BeamMesh *mesh);

/* Index helpers */
static inline int dof_w(int node)     { return 2 * node; }
static inline int dof_theta(int node) { return 2 * node + 1; }

#endif
