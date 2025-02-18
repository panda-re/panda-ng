cmake_minimum_required(VERSION 3.0.2)

project(libosi)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Read version number from command line argument
if(DEFINED VERSION)
    string(REGEX MATCH "v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${VERSION})
    set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_MATCH_2})
    set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_MATCH_3})
else()
    set(CPACK_PACKAGE_VERSION_MAJOR "0")
    set(CPACK_PACKAGE_VERSION_MINOR "0")
    set(CPACK_PACKAGE_VERSION_PATCH "0")
endif()

# find glib and link to it
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB_PKG REQUIRED glib-2.0)

option(ENABLE_TESTING "Build and run tests" OFF)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

set(IOHAL_SRCS src/iohal/memory/pmem_snapshot.cc
    src/iohal/translators/virtual_memory_translators.cc
    src/iohal/translators/vm_amd64.h src/iohal/translators/vm_amd64.cc
    src/iohal/translators/vm_i386.h  src/iohal/translators/vm_i386.cc
    src/iohal/translators/vm_i386_pae.h  src/iohal/translators/vm_i386_pae.cc
    src/iohal/memory/virtual_memory.cc)

# Compile all the .o files once
add_library(iohal-obj OBJECT ${IOHAL_SRCS})
target_include_directories(iohal-obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(iohal-obj PROPERTIES POSITION_INDEPENDENT_CODE 1)

# Then package them into a shared library and a static library
add_library(iohal SHARED $<TARGET_OBJECTS:iohal-obj>)
target_include_directories(iohal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

add_library(iohal-static STATIC $<TARGET_OBJECTS:iohal-obj>)
target_include_directories(iohal-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

set(OFFSET_SRCS src/offset/core.cc
            src/offset/profiles/win_2000_x86.h src/offset/profiles/win_2000_x86.cpp
            src/offset/profiles/win_xpsp2_x86.h src/offset/profiles/win_xpsp2_x86.cpp
            src/offset/profiles/win_xpsp3_x86.h src/offset/profiles/win_xpsp3_x86.cpp
            src/offset/profiles/win7_sp0_x86.h src/offset/profiles/win7_sp0_x86.cpp
            src/offset/profiles/win7_sp1_x86.h src/offset/profiles/win7_sp1_x86.cpp
            src/offset/profiles/win7_sp0_x64.h src/offset/profiles/win7_sp0_x64.cpp
            src/offset/profiles/win7_sp1_x64.h src/offset/profiles/win7_sp1_x64.cpp
            src/offset/profiles/linux3_16_x86.h src/offset/profiles/linux3_16_x86.cpp
            src/offset/profiles/linux3_16_x64.h src/offset/profiles/linux3_16_x64.cpp)

# Compile all the .o files once
add_library(offset-obj OBJECT ${OFFSET_SRCS})
target_include_directories(offset-obj PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/offset>
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(offset-obj PROPERTIES POSITION_INDEPENDENT_CODE 1)

# Then package them into a shared library and a static library
add_library(offset SHARED $<TARGET_OBJECTS:offset-obj>)
target_include_directories(offset PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/offset>
    $<INSTALL_INTERFACE:include>)

add_library(offset-static STATIC $<TARGET_OBJECTS:offset-obj>)
target_include_directories(offset-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/offset>
    $<INSTALL_INTERFACE:include>)

set(OSI_SRCS src/osi/windows/manager.cc
	        src/osi/windows/kernel_osi.cc
                src/osi/windows/api.cc
                src/osi/windows/windows_handles.cc
                src/osi/windows/mem_pe/mem_pe.cc
                src/osi/linux/api.cc
                src/osi/linux/kernel_osi.cc)

# Compile all the .o files once
add_library(osi-obj OBJECT ${OSI_SRCS})
target_include_directories(osi-obj PRIVATE ${GLIB_PKG_INCLUDE_DIRS})
target_include_directories(osi-obj PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(osi-obj PROPERTIES POSITION_INDEPENDENT_CODE 1)

# Then package them into a shared library and a static library
add_library(osi SHARED $<TARGET_OBJECTS:osi-obj>)
target_include_directories(osi PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)

add_library(osi-static STATIC $<TARGET_OBJECTS:osi-obj>)
target_include_directories(osi-static PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)

target_link_directories(osi PUBLIC ${GLIB_PKG_LIBRARY_DIRS})
target_link_libraries(osi offset iohal ${GLIB_PKG_LIBRARIES})

# Install everything
install(TARGETS osi osi-static offset offset-static iohal iohal-static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Install the headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h")

if (ENABLE_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

include(CPackConfig.txt)
