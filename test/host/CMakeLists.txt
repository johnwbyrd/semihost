# ZBC Host Tests
#
# Tests that run on the build machine (not cross-compiled)

# Main test executable
add_executable(zbc_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/test_harness.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/mock_device.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/mock_memory.c
    test_client_builder.c
    test_roundtrip.c
    test_api.c
    test_ansi_insecure.c
    test_ansi_secure.c
    test_main.c
)

# Include paths
target_include_directories(zbc_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Tests need both client and host definitions
target_compile_definitions(zbc_tests PRIVATE ZBC_CLIENT ZBC_HOST)

# Link against the semihosting libraries
target_link_libraries(zbc_tests
    zbc_semi_client
    zbc_semi_host
)

# Register test with CTest
add_test(NAME zbc_semihost_tests COMMAND zbc_tests)

# Sandbox tests - separate binary due to fork() requirement for test isolation
# (seccomp activation is irreversible within a process)
add_executable(zbc_sandbox_tests test_sandbox.c)
target_include_directories(zbc_sandbox_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zbc_sandbox_tests zbc_semi_host)

# Pass through seccomp define from parent
if(ZBC_USE_SECCOMP)
    target_compile_definitions(zbc_sandbox_tests PRIVATE ZBC_HAVE_SECCOMP)
    target_link_libraries(zbc_sandbox_tests ${SECCOMP_LIBRARIES})
endif()

add_test(NAME zbc_sandbox_tests COMMAND zbc_sandbox_tests)
