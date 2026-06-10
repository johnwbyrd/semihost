# Host tests for the C++ library. Exercises the C++ host (zbc_semi_hostpp)
# while encoding requests with the C client (zbc_semi_client) — the two must
# agree on the wire protocol.

add_executable(zbc_cpp_tests test_cpp.cpp)
target_link_libraries(zbc_cpp_tests zbc_semi_hostpp zbc_semi_client)
add_test(NAME zbc_cpp_tests COMMAND zbc_cpp_tests)
