#include "namc/core.h"

#include <stddef.h>

int namc_core_get_version(namc_core_version_t *out_version)
{
    if (out_version == NULL) {
        return 0;
    }

    out_version->major = NAMC_CORE_VERSION_MAJOR;
    out_version->minor = NAMC_CORE_VERSION_MINOR;
    out_version->patch = NAMC_CORE_VERSION_PATCH;
    return 1;
}

const char *namc_core_version_string(void)
{
    return NAMC_CORE_VERSION_STRING;
}
