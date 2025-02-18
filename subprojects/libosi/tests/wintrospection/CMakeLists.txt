add_executable(test-basic test_basic.cc)
target_link_libraries(test-basic wintrospection gtest_main offset iohal)
add_test(Wintrospection test-basic)

include(ExternalData)

# replace external with a place where tests will be hosted
set(ExternalData_URL_TEMPLATES "http://external/testdata/%(algo)/%(hash)")
set(ExternalData_TIMEOUT_ABSOLUTE 0)

macro(add_file_test testname testpath testdata)
    add_executable(${testname} ${testpath})
    target_link_libraries(${testname} osi
    				      iohal offset gtest_main)
	ExternalData_Add_Test(TestFiles
  		NAME ${testname}-test
		COMMAND ${testname} DATA{${testdata}})
endmacro()

add_file_test(test-i_t test_i_t.cc amd64.raw)
add_file_test(test-plist-amd64 test_plist_amd64.cc amd64.raw)
add_file_test(test-modlist-amd64 test_modlist_amd64.cc amd64.raw)
add_file_test(test-modlist-i386 test_modlist_i386.cc i386.raw)
add_file_test(test-kernbase-amd64 test_kernbase_amd64.cc amd64.raw)
add_file_test(test-plist-i386 test_plist_i386.cc i386.raw)
add_file_test(test-kernbase-i386 test_kernbase_i386.cc i386.raw)

macro(add_groundtruth_file_test testname testpath testdata gtruthdata)
    add_executable(${testname} ${testpath})
    target_link_libraries(${testname} osi
    				      iohal offset gtest_main)
	ExternalData_Add_Test(TestFiles
  		NAME ${testname}-test
		COMMAND ${testname} DATA{${testdata}} DATA{${gtruthdata}})
endmacro()

add_groundtruth_file_test(test-modlist-wow64notepad-win7sp0
                          test_modlist_wow64_notepad_win7sp0.cc
                          snapshots/wow64_notepadwin7sp0.raw
                          dlllists/wow64_notepadwin7sp0_dlllist.json)
add_groundtruth_file_test(test-modlist-wow64notepad-win7sp1
                          test_modlist_wow64_notepad_win7sp1.cc
                          snapshots/wow64_notepadwin7sp1.raw
                          dlllists/wow64_notepadwin7sp1_dlllist.json)

add_groundtruth_file_test(test-modlist-wow64precalc-win7sp0
                          test_modlist_wow64_precalc_win7sp0.cc
                          snapshots/wow64_precalcwin7sp0.raw
                          dlllists/wow64_precalcwin7sp0_dlllist.json)
add_groundtruth_file_test(test-modlist-wow64precalc-win7sp1
                          test_modlist_wow64_precalc_win7sp1.cc
                          snapshots/wow64_precalcwin7sp1.raw
                          dlllists/wow64_precalcwin7sp1_dlllist.json)

add_groundtruth_file_test(test-modlist-wow64windbg-win7sp0
                          test_modlist_wow64_windbg_win7sp0.cc
                          snapshots/wow64_windbgwin7sp0.raw
                          dlllists/wow64_windbgwin7sp0_dlllist.json)
add_groundtruth_file_test(test-modlist-wow64windbg-win7sp1
                          test_modlist_wow64_windbg_win7sp1.cc
                          snapshots/wow64_windbgwin7sp1.raw
                          dlllists/wow64_windbgwin7sp1_dlllist.json)

ExternalData_Add_Target(TestFiles)
