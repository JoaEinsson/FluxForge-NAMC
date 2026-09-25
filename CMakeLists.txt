cmake_minimum_required(VERSION 3.20)

project(FluxForge-NAMC VERSION 0.1.0 LANGUAGES C)

option(NAMC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(NamcWarnings)

add_library(namc_core STATIC
    src/core/core.c
    src/core/reference.c
)

target_compile_features(namc_core PUBLIC c_std_11)
target_include_directories(namc_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
set_target_properties(namc_core PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
)
namc_enable_strict_warnings(namc_core)

add_library(namc_core_binding SHARED
    src/core/core.c
    src/core/reference.c
)
target_compile_features(namc_core_binding PUBLIC c_std_11)
target_compile_definitions(namc_core_binding PRIVATE NAMC_CORE_SHARED_BUILD)
target_include_directories(namc_core_binding
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
set_target_properties(namc_core_binding PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
)
namc_enable_strict_warnings(namc_core_binding)

# The portable controller has no include path or dependency on plant truth.
add_library(namc_plant STATIC src/plant/linear_plant.c)
target_include_directories(namc_plant PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/plant/include")
target_link_libraries(namc_plant PUBLIC namc_core)
target_compile_features(namc_plant PUBLIC c_std_11)
set_target_properties(namc_plant PROPERTIES C_EXTENSIONS OFF C_STANDARD 11 C_STANDARD_REQUIRED YES)
namc_enable_strict_warnings(namc_plant)
if(UNIX)
    target_link_libraries(namc_core PUBLIC m)
    target_link_libraries(namc_core_binding PRIVATE m)
endif()

add_executable(namc_sim simulations/linear_reference.c)
target_link_libraries(namc_sim PRIVATE namc_plant)
set_target_properties(namc_sim PROPERTIES C_EXTENSIONS OFF C_STANDARD 11 C_STANDARD_REQUIRED YES)
namc_enable_strict_warnings(namc_sim)

# Provenance is a configure-time snapshot; reconfigure after source changes.
set(NAMC_REVISION "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NAMC_GIT_HEAD OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
        RESULT_VARIABLE NAMC_GIT_RESULT)
    if(NAMC_GIT_RESULT EQUAL 0 AND NAMC_GIT_HEAD MATCHES "^[0-9a-f]+$")
        set(NAMC_REVISION "${NAMC_GIT_HEAD}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE NAMC_GIT_DIRTY ERROR_QUIET RESULT_VARIABLE NAMC_STATUS_RESULT)
        if(NOT NAMC_STATUS_RESULT EQUAL 0)
            string(APPEND NAMC_REVISION "-status-unknown")
        elseif(NAMC_GIT_DIRTY)
            string(APPEND NAMC_REVISION "-dirty")
        endif()
    endif()
endif()
target_compile_definitions(namc_sim PRIVATE
    NAMC_BUILD_REVISION="${NAMC_REVISION}"
    NAMC_BUILD_COMPILER="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
    NAMC_BUILD_CONFIGURATION="$<CONFIG>")

include(CTest)

if(BUILD_TESTING)
    add_executable(namc_core_test
        tests/core_test.c
    )
    target_compile_features(namc_core_test PRIVATE c_std_11)
    set_target_properties(namc_core_test PROPERTIES
        C_EXTENSIONS OFF
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
    )
    target_link_libraries(namc_core_test PRIVATE namc_core)
    namc_enable_strict_warnings(namc_core_test)

    add_test(NAME namc_core_api COMMAND namc_core_test)

    add_executable(namc_reference_test tests/reference_test.c)
    target_link_libraries(namc_reference_test PRIVATE namc_plant)
    set_target_properties(namc_reference_test PROPERTIES C_EXTENSIONS OFF C_STANDARD 11 C_STANDARD_REQUIRED YES)
    namc_enable_strict_warnings(namc_reference_test)
    add_test(NAME namc_linear_reference COMMAND namc_reference_test)
    add_test(NAME namc_sim_smoke COMMAND namc_sim)

    option(NAMC_REQUIRE_PYTEST "Fail configuration when pytest is unavailable" OFF)
    find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
    set(NAMC_HAS_PYTEST FALSE)
    if(Python3_Interpreter_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m pytest --version
            RESULT_VARIABLE NAMC_PYTEST_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NAMC_PYTEST_RESULT EQUAL 0)
            set(NAMC_HAS_PYTEST TRUE)
        endif()
    endif()

    if(NAMC_HAS_PYTEST)
        add_test(
            NAME namc_python_binding
            COMMAND "${Python3_EXECUTABLE}" -m pytest -q
                "${CMAKE_CURRENT_SOURCE_DIR}/python/tests"
        )
        set_tests_properties(namc_python_binding PROPERTIES
            ENVIRONMENT
                "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/python;NAMC_CORE_LIBRARY=$<TARGET_FILE:namc_core_binding>;NAMC_SIM_EXECUTABLE=$<TARGET_FILE:namc_sim>"
        )
    elseif(NAMC_REQUIRE_PYTEST)
        message(FATAL_ERROR "Python 3.10+ with pytest is required")
    else()
        message(STATUS "Python 3.10+ with pytest not found; skipping Python binding test")
    endif()
endif()
