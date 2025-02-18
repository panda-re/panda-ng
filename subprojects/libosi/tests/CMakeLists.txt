set(GTEST_SRC_DIR "/usr/src/gtest" CACHE PATH "Path to google test source")
if (NOT EXISTS ${GTEST_SRC_DIR})
    message(WARNING "Could not find ${GTEST_SRC_DIR}")
    message(WARNING "Please install libgtest-dev to enable testing")
else()
include(ExternalProject)
ExternalProject_Add(googletest
    SOURCE_DIR        "/usr/src/gtest"
    BINARY_DIR        "${CMAKE_BINARY_DIR}/googletest-build"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
)

add_subdirectory(/usr/src/gtest
                 ${CMAKE_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

add_subdirectory(iohal)
add_subdirectory(offset)
# add_subdirectory(wintrospection)
add_executable(winvtop vtop.cc)
target_include_directories(winvtop PUBLIC $<TARGET_PROPERTY:osi,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(winvtop iohal offset osi)
add_dependencies(winvtop iohal offset osi)

endif()
