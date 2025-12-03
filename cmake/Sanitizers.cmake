# Sanitizers.cmake - Configure AddressSanitizer and UndefinedBehaviorSanitizer
#
# Usage:
#   cmake -B build -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
#
# Requires Clang or GCC. MSVC has limited sanitizer support.

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(ENABLE_SANITIZERS)
    if(MSVC)
        message(WARNING "MSVC sanitizer support is limited. Consider using Clang.")
        # MSVC only supports AddressSanitizer, not UBSan
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    else()
        # Clang and GCC
        set(SANITIZER_FLAGS
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
        )
        add_compile_options(${SANITIZER_FLAGS})
        add_link_options(${SANITIZER_FLAGS})

        # Additional UBSan options for better diagnostics
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            add_compile_options(-fno-sanitize-recover=undefined)
        endif()
    endif()

    message(STATUS "Sanitizers enabled: AddressSanitizer + UndefinedBehaviorSanitizer")
endif()
