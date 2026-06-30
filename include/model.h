#ifndef MODEL_H
#define MODEL_H

/*
 * Sign / unit conventions used throughout this solver:
 *  - "level" means a real-world elevation (e.g. m w.r.t. a fixed datum),
 *    higher numbers = higher up. Levels DECREASE going down.
 *  - Horizontal displacement w is positive to the RIGHT.
 *  - A soil profile's "top_level" is the ground surface on that side.
 *    Above top_level there is no soil on that side (e.g. an excavation),
 *    and the soil contributes zero pressure there.
 *  - Pressures p are in force/length^2 (e.g. kN/m2), acting per running
 *    metre of wall, positive meaning "pushes the wall away from this side".
 *  - Stiffnesses k1,k2,k3 are secant stiffnesses (kN/m2 per m of relative
 *    displacement), describing how pressure changes between the at-rest
 *    (neutral) state and the active/passive limit. k1 applies to the
 *    branch closest to the neutral state, k2 the next, k3 the branch
 *    closest to the limit pressure. Soil is allowed to soften (k1>k2>k3)
 *    or not; the solver does not assume monotonic ordering.
 */

typedef struct {
    double top;        /* top level of this layer    */
    double bottom;      /* bottom level of this layer  */
    double gamma;        /* unit weight                 */
    double phi_deg;       /* friction angle (degrees)     */
    double cohesion;       /* cohesion                      */
    double k1, k2, k3;      /* secant stiffnesses, neutral->limit */
} SoilLayer;

typedef struct {
    SoilLayer *layers;
    int n_layers;
    double top_level;    /* ground level on this side (== layers[0].top) */
} SoilProfile;

typedef struct {
    double EI;          /* bending stiffness, force*length^2 */
    double top_level;
    double tip_level;
} SheetPile;

typedef enum {
    SUPPORT_FIXED_HORIZONTAL
} SupportType;

typedef struct {
    double level;
    SupportType type;
} Support;

typedef struct {
    SheetPile pile;
    SoilProfile left;
    SoilProfile right;
    Support *supports;
    int n_supports;
} Project;

#endif
