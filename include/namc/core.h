#ifndef NAMC_CORE_H
#define NAMC_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(NAMC_CORE_SHARED_BUILD)
#define NAMC_CORE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(NAMC_CORE_SHARED_USE)
#define NAMC_CORE_API __declspec(dllimport)
#else
#define NAMC_CORE_API
#endif

/*
 * This metadata API is experimental and pre-1.0. API and ABI compatibility
 * are not promised. It intentionally exposes no motor-control behavior.
 */
#define NAMC_CORE_API_EXPERIMENTAL 1U
#define NAMC_CORE_VERSION_MAJOR 0U
#define NAMC_CORE_VERSION_MINOR 1U
#define NAMC_CORE_VERSION_PATCH 0U
#define NAMC_CORE_VERSION_STRING "0.1.0"

typedef struct namc_core_version {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
} namc_core_version_t;

/*
 * Copy the current core version into out_version.
 *
 * Returns 1 when out_version is valid, or 0 when out_version is NULL.
 */
NAMC_CORE_API int namc_core_get_version(namc_core_version_t *out_version);

/*
 * Return the current version as a null-terminated string owned by the core.
 * The returned pointer remains valid for the lifetime of the process.
 */
NAMC_CORE_API const char *namc_core_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* NAMC_CORE_H */
