# Conformance suite: the C host and the C++ host must exhibit identical
# observable behavior for the same requests. Links BOTH host libraries.

add_executable(zbc_conformance_tests test_conformance.cpp)
target_include_directories(zbc_conformance_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(zbc_conformance_tests zbc_semi_host zbc_semi_hostpp)
add_test(NAME zbc_conformance_tests COMMAND zbc_conformance_tests)
