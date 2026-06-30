#ifndef SOIL_SPRINGS_H
#define SOIL_SPRINGS_H

#include "model.h"

/* Vertical (overburden) stress at `level` for one soil profile.
 * Returns 0 if level is at or above the profile's top_level. */
double soil_vertical_stress(const SoilProfile *profile, double level);

/* Rankine earth pressure coefficients from friction angle (degrees). */
double soil_Ka(double phi_deg);
double soil_Kp(double phi_deg);
double soil_K0(double phi_deg);

/* Limit and at-rest pressures at a given level, using the soil layer
 * that contains that level. */
typedef struct {
    double p0;   /* at-rest (neutral) pressure   */
    double pa;   /* active limit pressure (>=0)  */
    double pp;   /* passive limit pressure       */
    double k1, k2, k3; /* secant stiffnesses of the governing layer */
} SoilPressureState;

/* Fills `state` for `profile` at `level`. If level is above the profile's
 * top (no soil there), all pressures are zero and stiffnesses are zero. */
void soil_pressure_state(const SoilProfile *profile, double level,
                          SoilPressureState *state);

/* Evaluates the tri-linear reaction curve for ONE side.
 *  w_eff > 0 means the wall is moving INTO this soil (toward the passive
 *            limit pp), w_eff < 0 means moving AWAY from it (toward pa).
 * Returns the pressure p(w_eff) and its tangent dp/dw_eff in *dpdw. */
double soil_branch_pressure(const SoilPressureState *state, double w_eff,
                             double *dpdw);

#endif
