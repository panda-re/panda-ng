add_executable(test-offset-basic ${CMAKE_CURRENT_SOURCE_DIR}/test_basic.cc)
target_link_libraries(test-offset-basic offset-static gtest_main)
add_test(TestBasicOffsets test-offset-basic)
