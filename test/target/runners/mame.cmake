# MAME Runner for ZBC Target Tests
#
# Detects MAME and provides add_mame_test() function.

# Find MAME executable
find_program(MAME_EXECUTABLE mame
    HINTS
        $ENV{MAME_PATH}
        ${MAME_PATH}
        $ENV{HOME}/git/mame
        $ENV{HOME}/mame
        /opt/mame
        /usr/local/bin
        /usr/bin
    DOC "MAME emulator executable"
)

if(MAME_EXECUTABLE)
    set(ZBC_RUNNER_MAME_FOUND TRUE)
    message(STATUS "MAME runner: found at ${MAME_EXECUTABLE}")
else()
    set(ZBC_RUNNER_MAME_FOUND FALSE)
    message(STATUS "MAME runner: not found (set MAME_PATH or install MAME)")
endif()

# Semihost share directory
if(NOT ZBC_MAME_SHARE_DIR)
    set(ZBC_MAME_SHARE_DIR "$ENV{HOME}/.mame/semihost")
endif()

#
# add_mame_test(name elf_path machine [TIMEOUT timeout] [INPUT input_string])
#
# Registers a CTest test that runs an ELF binary on MAME.
#
# Arguments:
#   name        - Test name for CTest
#   elf_path    - Path to ELF binary
#   machine     - MAME machine name (e.g., zbci386, zbcm6502)
#   TIMEOUT     - Test timeout in seconds (default: 30)
#   INPUT       - String to pipe to stdin for READC testing
#
function(add_mame_test name elf_path machine)
    if(NOT MAME_EXECUTABLE)
        message(STATUS "Skipping MAME test ${name} - MAME not found")
        return()
    endif()

    cmake_parse_arguments(ARG "" "TIMEOUT;INPUT" "" ${ARGN})

    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 30)
    endif()

    # Ensure share directory exists
    file(MAKE_DIRECTORY ${ZBC_MAME_SHARE_DIR})

    # Build the command
    # We use sh -c to enable stdin piping for READC tests
    if(ARG_INPUT)
        set(TEST_COMMAND
            sh -c "echo '${ARG_INPUT}' | ${MAME_EXECUTABLE} ${machine} -window -skip_gameinfo -share_directory ${ZBC_MAME_SHARE_DIR} -elfload ${elf_path} -seconds_to_run 15"
        )
    else()
        set(TEST_COMMAND
            ${MAME_EXECUTABLE} ${machine}
            -window
            -skip_gameinfo
            -share_directory ${ZBC_MAME_SHARE_DIR}
            -elfload ${elf_path}
            -seconds_to_run 15
        )
    endif()

    add_test(
        NAME ${name}
        COMMAND ${TEST_COMMAND}
    )

    set_tests_properties(${name} PROPERTIES
        LABELS "target;mame;${machine}"
        TIMEOUT ${ARG_TIMEOUT}
        ENVIRONMENT "HOME=$ENV{HOME}"
    )

    message(STATUS "Registered MAME test: ${name} (${machine})")
endfunction()
