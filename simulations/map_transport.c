#include "map_transport.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double namc_id_axis[NAMC_FLUX_MAX_AXIS], namc_iq_axis[NAMC_FLUX_MAX_AXIS];
static double namc_psi_d[NAMC_FLUX_MAX_AXIS * NAMC_FLUX_MAX_AXIS];
static double namc_psi_q[NAMC_FLUX_MAX_AXIS * NAMC_FLUX_MAX_AXIS];
static char namc_source[128];

static int namc_read_token(char *token, size_t capacity)
{
    size_t length = 0U, skipped = 0U;
    int ch;
    do {
        ch = getchar();
        if (ch == EOF || ++skipped > 1024U) { return 0; }
    } while (isspace((unsigned char)ch));
    while (ch != EOF && !isspace((unsigned char)ch)) {
        if (length + 1U >= capacity) { return 0; }
        token[length++] = (char)ch;
        ch = getchar();
    }
    token[length] = '\0';
    return !ferror(stdin);
}

static int namc_read_number(double *out)
{
    char token[96], *end;
    if (!namc_read_token(token, sizeof(token))) {
        return 0;
    }
    errno = 0;
    *out = strtod(token, &end);
    return (errno == 0 || (errno == ERANGE && *out != 0.0)) &&
        end != token && *end == '\0' && isfinite(*out);
}

static int namc_read_integer(unsigned int maximum, unsigned int *out)
{
    double value;
    if (!namc_read_number(&value) || value < 0.0 || value > (double)maximum ||
        floor(value) != value) {
        return 0;
    }
    *out = (unsigned int)value;
    return 1;
}

int namc_sim_read_map(namc_flux_map_t *map)
{
    char magic[32];
    unsigned int nd, nq, evidence, source_length, byte;
    size_t i;
    int trailing;
    namc_flux_result_t status;
    namc_flux_limits_t limits;
    namc_flux_data_t data = {0};
    if (!namc_read_token(magic, sizeof(magic)) || strcmp(magic, "NAMC_FLUX_TRANSPORT_V1") != 0 ||
        !namc_read_integer(1U, &data.version) || data.version != 1U ||
        !namc_read_integer(1U, &data.units) || data.units != 1U ||
        !namc_read_integer(1U, &data.convention) || data.convention != 1U ||
        !namc_read_integer(NAMC_FLUX_MAX_AXIS, &nd) || nd < 2U ||
        !namc_read_integer(NAMC_FLUX_MAX_AXIS, &nq) || nq < 2U ||
        !namc_read_integer(4U, &evidence) || evidence == 0U ||
        !namc_read_integer(127U, &source_length) || source_length == 0U ||
        !namc_read_number(&data.temperature_k) ||
        !namc_read_number(&data.declared_flux_error_wb) ||
        !namc_read_number(&limits.min_incremental_h) ||
        !namc_read_number(&limits.max_reciprocity_error_h) ||
        !namc_read_number(&limits.min_rcond)) {
        return 0;
    }
    for (i = 0U; i < source_length; ++i) {
        if (!namc_read_integer(255U, &byte) || byte == 0U) {
            return 0;
        }
        namc_source[i] = (char)(unsigned char)byte;
    }
    namc_source[source_length] = '\0';
    data.nd = nd;
    data.nq = nq;
    data.count = (size_t)nd * nq;
    data.evidence = (namc_flux_evidence_t)evidence;
    data.source_id = namc_source;
    data.id_axis = namc_id_axis;
    data.iq_axis = namc_iq_axis;
    data.psi_d = namc_psi_d;
    data.psi_q = namc_psi_q;
    for (i = 0U; i < data.nd; ++i) {
        if (!namc_read_number(&namc_id_axis[i])) { return 0; }
    }
    for (i = 0U; i < data.nq; ++i) {
        if (!namc_read_number(&namc_iq_axis[i])) { return 0; }
    }
    for (i = 0U; i < data.count; ++i) {
        if (!namc_read_number(&namc_psi_d[i])) { return 0; }
    }
    for (i = 0U; i < data.count; ++i) {
        if (!namc_read_number(&namc_psi_q[i])) { return 0; }
    }
    /* No extra tokens, and no unbounded trailing whitespace scan. */
    for (i = 0U; i < 1024U; ++i) {
        trailing = getchar();
        if (trailing == EOF) { break; }
        if (!isspace((unsigned char)trailing)) { return 0; }
    }
    if (i == 1024U || ferror(stdin)) { return 0; }
    status = namc_flux_map_prepare(&data, &limits, map);
    if (status != NAMC_FLUX_OK) {
        fprintf(stderr, "Map preparation rejected: flux status %d.\n", (int)status);
        return 0;
    }
    return 1;
}

static void namc_print_array(const double *values, size_t count)
{
    size_t i;
    putchar('[');
    for (i = 0U; i < count; ++i) {
        printf("%s%.17g", i == 0U ? "" : ",", values[i]);
    }
    putchar(']');
}

static void namc_print_table(const double *values, size_t nd, size_t nq)
{
    size_t row;
    putchar('[');
    for (row = 0U; row < nd; ++row) {
        if (row != 0U) { putchar(','); }
        namc_print_array(values + row * nq, nq);
    }
    putchar(']');
}

void namc_sim_print_map(const namc_flux_map_t *map)
{
    const namc_flux_data_t *d = &map->data;
    const unsigned char *source = (const unsigned char *)d->source_id;
    const char *evidence[] = {"invalid", "analytical", "simulation", "fea", "measured"};
    printf("\"map_limits\": {\"min_incremental_h\": %.17g, "
        "\"max_reciprocity_error_h\": %.17g, \"min_rcond\": %.17g},\n",
        map->limits.min_incremental_h, map->limits.max_reciprocity_error_h,
        map->limits.min_rcond);
    printf("\"map\": {\"schema_version\": %u, \"units\": \"SI\", "
        "\"convention\": \"amplitude-invariant-dq\", \"evidence\": \"%s\", "
        "\"temperature_K\": %.17g, \"declared_flux_error_Wb\": %.17g, "
        "\"source_id_utf8_hex\": \"", d->version, evidence[d->evidence],
        d->temperature_k, d->declared_flux_error_wb);
    /* ASCII hex keeps arbitrary provenance bytes out of JSON syntax. The
     * Python wrapper restores UTF-8 text before returning its report. */
    while (*source != 0U) { printf("%02x", (unsigned int)*source++); }
    printf("\", \"id_A\": "); namc_print_array(d->id_axis, d->nd);
    printf(", \"iq_A\": "); namc_print_array(d->iq_axis, d->nq);
    printf(", \"psi_d_Wb\": "); namc_print_table(d->psi_d, d->nd, d->nq);
    printf(", \"psi_q_Wb\": "); namc_print_table(d->psi_q, d->nd, d->nq);
    putchar('}');
}
