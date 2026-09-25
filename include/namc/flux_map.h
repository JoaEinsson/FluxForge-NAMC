#ifndef NAMC_FLUX_MAP_H
#define NAMC_FLUX_MAP_H

#include "namc/reference.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental in-memory API, not a stable file format or ABI. */
#define NAMC_FLUX_MAP_VERSION 1U
#define NAMC_FLUX_MAX_AXIS 256U
#define NAMC_FLUX_SI_UNITS 1U
#define NAMC_FLUX_AMPLITUDE_INVARIANT_DQ 1U

typedef enum namc_flux_result {
    NAMC_FLUX_OK = 0,
    NAMC_FLUX_INVALID_INPUT,
    NAMC_FLUX_INVALID_MAP,
    NAMC_FLUX_OUT_OF_DOMAIN,
    NAMC_FLUX_ILL_CONDITIONED,
    NAMC_FLUX_NON_RECIPROCAL,
    NAMC_FLUX_NON_PASSIVE
} namc_flux_result_t;

typedef enum namc_flux_evidence {
    NAMC_FLUX_ANALYTICAL = 1,
    NAMC_FLUX_SIMULATED,
    NAMC_FLUX_FEA,
    NAMC_FLUX_MEASURED
} namc_flux_evidence_t;

typedef struct namc_flux_data {
    unsigned int version, units, convention;
    size_t nd, nq, count;
    const double *id_axis, *iq_axis; /* Strictly increasing, A. */
    const double *psi_d, *psi_q; /* Wb, index = d_index * nq + q_index. */
    namc_flux_evidence_t evidence;
    const char *source_id; /* Nonempty, at most 127 bytes, valid for map lifetime. */
    double temperature_k;
    double declared_flux_error_wb; /* Source-declared bound, NOT verified here. */
} namc_flux_data_t;

typedef struct namc_flux_limits {
    double min_incremental_h; /* Symmetric part must exceed this positive floor. */
    double max_reciprocity_error_h; /* Allowed |d(psi_d)/diq - d(psi_q)/did|. */
    double min_rcond; /* Reciprocal infinity-norm condition number, (0, 1]. */
} namc_flux_limits_t;

typedef struct namc_flux_jacobian {
    double dd, dq, qd, qq; /* Exact derivatives of the cell interpolants, H. */
} namc_flux_jacobian_t;

/* Treat prepared objects as read-only. They borrow immutable arrays/metadata:
 * keep all storage alive, and re-prepare after any modification. This is not
 * a memory-corruption detector or a model activation/rollback mechanism. */
typedef struct namc_flux_map {
    namc_flux_data_t data;
    namc_flux_limits_t limits;
    unsigned int prepared;
} namc_flux_map_t;

/* Offline-only: validate every node and both sides of every cell boundary.
 * Failure invalidates out; inputs must not alias out. No allocation/copy of
 * tables. Sizes must match actual caller-owned storage (as in any C array API). */
NAMC_CORE_API namc_flux_result_t namc_flux_map_prepare(
    const namc_flux_data_t *data, const namc_flux_limits_t *limits, namc_flux_map_t *out);

/* Bounded lookup, no extrapolation/clamping. Optional Jacobian; flux required.
 * Interior knots use the higher-current cell, final knots the last cell.
 * Outputs are unchanged on error. Angles/temperature are not map dimensions. */
NAMC_CORE_API namc_flux_result_t namc_flux_map_lookup(const namc_flux_map_t *map,
    namc_dq_t current, namc_dq_t *flux, namc_flux_jacobian_t *jacobian);

/* J * rate = rhs; scaled solve checks determinant and conditioning. rhs is
 * d(psi)/dt (V), rate is A/s. Output unchanged on error; never decouples J. */
NAMC_CORE_API namc_flux_result_t namc_flux_solve(namc_flux_jacobian_t jacobian,
    double min_rcond, namc_dq_t rhs, namc_dq_t *rate);

/* Sinusoidal dq torque, N m; does not require a Jacobian. */
NAMC_CORE_API namc_flux_result_t namc_flux_torque(namc_dq_t flux,
    namc_dq_t current, unsigned int pole_pairs, double *torque);

#ifdef __cplusplus
}
#endif
#endif /* NAMC_FLUX_MAP_H */
