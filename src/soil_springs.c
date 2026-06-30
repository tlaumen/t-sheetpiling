#include "soil_springs.h"
#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double soil_vertical_stress(const SoilProfile *profile, double level)
{
    if (level >= profile->top_level) return 0.0;

    double sigma = 0.0;
    for (int i = 0; i < profile->n_layers; i++) {
        const SoilLayer *L = &profile->layers[i];
        if (level >= L->bottom) {
            double thickness = L->top - level;
            if (thickness > 0.0) sigma += thickness * L->gamma;
            break;
        } else {
            double thickness = L->top - L->bottom;
            sigma += thickness * L->gamma;
        }
    }
    return sigma;
}

double soil_Ka(double phi_deg)
{
    double phi = phi_deg * M_PI / 180.0;
    return (1.0 - sin(phi)) / (1.0 + sin(phi));
}

double soil_Kp(double phi_deg)
{
    double phi = phi_deg * M_PI / 180.0;
    return (1.0 + sin(phi)) / (1.0 - sin(phi));
}

double soil_K0(double phi_deg)
{
    double phi = phi_deg * M_PI / 180.0;
    return 1.0 - sin(phi);
}

static const SoilLayer *find_layer(const SoilProfile *profile, double level)
{
    for (int i = 0; i < profile->n_layers; i++) {
        const SoilLayer *L = &profile->layers[i];
        if (level <= L->top && level >= L->bottom) return L;
    }
    if (profile->n_layers > 0) return &profile->layers[profile->n_layers - 1];
    return NULL;
}

void soil_pressure_state(const SoilProfile *profile, double level,
                          SoilPressureState *state)
{
    state->p0 = state->pa = state->pp = 0.0;
    state->k1 = state->k2 = state->k3 = 0.0;

    if (level >= profile->top_level) return; /* no soil here (excavated/open) */

    const SoilLayer *L = find_layer(profile, level);
    if (!L) return;

    double sigma_v = soil_vertical_stress(profile, level);

    double Ka = soil_Ka(L->phi_deg);
    double Kp = soil_Kp(L->phi_deg);
    double K0 = soil_K0(L->phi_deg);

    double pa = Ka * sigma_v - 2.0 * L->cohesion * sqrt(Ka);
    if (pa < 0.0) pa = 0.0; /* soil cannot sustain tension */
    double pp = Kp * sigma_v + 2.0 * L->cohesion * sqrt(Kp);
    double p0 = K0 * sigma_v;

    state->p0 = p0;
    state->pa = pa;
    state->pp = pp;
    state->k1 = L->k1;
    state->k2 = L->k2;
    state->k3 = L->k3;
}

/* One-sided tri-linear backbone from p0 toward p_limit (p_limit may be
 * above OR below p0 -- passive limits are above, active limits are
 * usually below), parameterised by displacement magnitude w >= 0 (w=0 at
 * neutral). The pressure span |p_limit - p0| is split into three EQUAL
 * PRESSURE increments; branch i (stiffness k_i) covers increment i, so
 * its displacement span is (|delta|/3)/k_i. This ties the breakpoints to
 * the physical limit pressure rather than an arbitrary length scale.
 * Returns p(w) and sets *dpdw = dp/dw (signed: negative on a softening
 * branch that is losing pressure, e.g. moving toward an active limit). */
static double eval_branch(double p0, double p_limit,
                           double k1, double k2, double k3,
                           double w, double *dpdw)
{
    double delta = p_limit - p0;
    double adelta = fabs(delta);
    if (adelta < 1e-9) { *dpdw = 0.0; return p0; }
    double sign = (delta > 0.0) ? 1.0 : -1.0;

    double ks[3] = {k1, k2, k3};
    for (int i = 0; i < 3; i++) if (ks[i] <= 0.0) ks[i] = 1e-9;

    double seg = adelta / 3.0;
    double w1 = seg / ks[0];
    double w2 = w1 + seg / ks[1];
    double w3 = w2 + seg / ks[2];

    if (w <= 0.0) { *dpdw = sign * ks[0]; return p0; }
    if (w <= w1)  { *dpdw = sign * ks[0]; return p0 + sign * ks[0] * w; }
    if (w <= w2)  { *dpdw = sign * ks[1]; return p0 + sign * seg + sign * ks[1] * (w - w1); }
    if (w <= w3)  { *dpdw = sign * ks[2]; return p0 + sign * 2.0 * seg + sign * ks[2] * (w - w2); }
    *dpdw = 0.0;
    return p_limit;
}

double soil_branch_pressure(const SoilPressureState *state, double w_eff,
                             double *dpdw)
{
    if (w_eff >= 0.0) {
        return eval_branch(state->p0, state->pp,
                            state->k1, state->k2, state->k3,
                            w_eff, dpdw);
    } else {
        double dpdu;
        double p = eval_branch(state->p0, state->pa,
                                state->k1, state->k2, state->k3,
                                -w_eff, &dpdu);
        /* p = f(u), u = -w_eff  =>  dp/dw_eff = -f'(u) */
        *dpdw = -dpdu;
        return p;
    }
}
