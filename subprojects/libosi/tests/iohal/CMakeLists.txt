add_executable(test-sparsemem ${CMAKE_CURRENT_SOURCE_DIR}/sparse_pmem.h
                             ${CMAKE_CURRENT_SOURCE_DIR}/sparse_pmem.cc
                             ${CMAKE_CURRENT_SOURCE_DIR}/test_sparse_pmem.cc)
target_link_libraries(test-sparsemem iohal-static gtest_main)
add_test(TestSparseMemory test-sparsemem)

# test 32-bit memory translation
add_executable(test-ia32-trans ${CMAKE_CURRENT_SOURCE_DIR}/sparse_pmem.h
                               ${CMAKE_CURRENT_SOURCE_DIR}/sparse_pmem.cc
                               ${CMAKE_CURRENT_SOURCE_DIR}/test_vmtrans_ia32.cc)
target_link_libraries(test-ia32-trans iohal-static gtest_main)
add_test(TestIA32Translation test-ia32-trans)

# test 64-bit memory translation
add_executable(test-amd64-trans ${CMAKE_CURRENT_SOURCE_DIR}/sparse_pmem.h
                               ${CMAKE_CURRENT_SOURCE_DIR}/sparse_pmem.cc
                               ${CMAKE_CURRENT_SOURCE_DIR}/test_vmtrans_amd64.cc)
target_link_libraries(test-amd64-trans iohal-static gtest_main)
add_test(TestAmd64Translation test-amd64-trans)


