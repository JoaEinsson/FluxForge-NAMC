#include "namc/core.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *message)
{
    (void)fprintf(stderr, "namc_core_test: %s\n", message);
    return 1;
}

int main(void)
{
    namc_core_version_t version = {0U, 0U, 0U};
    const char *version_string;

#if NAMC_CORE_API_EXPERIMENTAL != 1U
#error "API must remain marked experimental"
#endif
    if (namc_core_get_version(&version) != 1) {
        return fail("version query rejected a valid output pointer");
    }
    if (version.major != NAMC_CORE_VERSION_MAJOR ||
        version.minor != NAMC_CORE_VERSION_MINOR ||
        version.patch != NAMC_CORE_VERSION_PATCH) {
        return fail("structured version does not match public version macros");
    }
    if (namc_core_get_version(NULL) != 0) {
        return fail("version query accepted a NULL output pointer");
    }

    version_string = namc_core_version_string();
    if (version_string == NULL ||
        strcmp(version_string, NAMC_CORE_VERSION_STRING) != 0) {
        return fail("version string does not match public version macro");
    }

    return 0;
}
