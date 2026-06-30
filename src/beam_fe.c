#include "beam_fe.h"
#include <stdlib.h>
#include <string.h>

int beam_mesh_build(const SheetPile *pile, int n_nodes, BeamMesh *mesh)
{
    if (n_nodes < 2) return -1;

    mesh->n_nodes = n_nodes;
    mesh->ndof = 2 * n_nodes;
    double length = pile->top_level - pile->tip_level;
    mesh->dz = length / (n_nodes - 1);

    mesh->z = (double *)malloc(sizeof(double) * n_nodes);
    mesh->trib = (double *)malloc(sizeof(double) * n_nodes);
    mesh->K = (double *)calloc((size_t)mesh->ndof * mesh->ndof, sizeof(double));
    if (!mesh->z || !mesh->trib || !mesh->K) return -1;

    for (int i = 0; i < n_nodes; i++) {
        mesh->z[i] = pile->top_level - i * mesh->dz;
        mesh->trib[i] = mesh->dz; /* interior weight */
    }
    mesh->trib[0] *= 0.5;
    mesh->trib[n_nodes - 1] *= 0.5;

    double EI = pile->EI;
    double L = mesh->dz;
    double L2 = L * L, L3 = L2 * L;

    /* standard 2-node Euler-Bernoulli element stiffness, DOFs [w1,th1,w2,th2] */
    double ke[4][4] = {
        { 12.0/L3,  6.0/L2, -12.0/L3,  6.0/L2},
        {  6.0/L2,  4.0/L,   -6.0/L2,  2.0/L },
        {-12.0/L3, -6.0/L2,  12.0/L3, -6.0/L2},
        {  6.0/L2,  2.0/L,   -6.0/L2,  4.0/L }
    };
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < 4; b++)
            ke[a][b] *= EI;

    int ndof = mesh->ndof;
    for (int e = 0; e < n_nodes - 1; e++) {
        int gidx[4] = { dof_w(e), dof_theta(e), dof_w(e + 1), dof_theta(e + 1) };
        for (int a = 0; a < 4; a++)
            for (int b = 0; b < 4; b++)
                mesh->K[(size_t)gidx[a] * ndof + gidx[b]] += ke[a][b];
    }

    return 0;
}

void beam_mesh_free(BeamMesh *mesh)
{
    free(mesh->z);
    free(mesh->trib);
    free(mesh->K);
    mesh->z = NULL;
    mesh->trib = NULL;
    mesh->K = NULL;
}
