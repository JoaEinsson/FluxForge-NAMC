function(namc_enable_strict_warnings target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot configure warnings for unknown target: ${target}")
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options("${target}" PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wstrict-prototypes
        )
        if(NAMC_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE -Werror)
        endif()
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options("${target}" PRIVATE /W4)
        if(NAMC_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE /WX)
        endif()
    else()
        message(WARNING
            "No strict warning profile is configured for C compiler "
            "${CMAKE_C_COMPILER_ID}; continuing without compiler-specific flags."
        )
    endif()
endfunction()
