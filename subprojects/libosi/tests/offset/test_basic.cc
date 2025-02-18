#include "offset/offset.h"
#include "gtest/gtest.h"

TEST(BasicTest, TypeLibraryLoading)
{
    std::string supported_libraries[] = {"windows-64-7sp1", "windows-64-7sp0",
                                         "windows-32-7sp1", "windows-32-7sp0"};
    for (auto& library : supported_libraries) {
        auto tlib = load_type_library(library.c_str());
        ASSERT_TRUE(tlib != nullptr) << "Could not locate supported profile";
    }

    auto doesnt_exist = load_type_library("notreal-64-7sp1");
    ASSERT_TRUE(doesnt_exist == nullptr) << "Found a library that didn't exist!";
}

TEST(BasicTest, BasicTranslation)
{
    auto tlib = load_type_library("windows-64-7sp1");
    ASSERT_TRUE(tlib != nullptr) << "Could not locate requested profile";

    auto st = translate(tlib, "_EPROCESS");
    ASSERT_TRUE(is_valid_structure_type(st)) << "Could not locate _EPROCESS type";

    auto st_repeat = translate(tlib, "_EPROCESS");
    ASSERT_TRUE(is_valid_structure_type(st_repeat))
        << "Could not lookup the same type twice";

    auto st2 = translate(tlib, "MyType");
    ASSERT_FALSE(is_valid_structure_type(st2)) << "Found a type that didn't exist";
}

TEST(BasicTest, BasicWin7x64sp1)
{
    auto tlib = load_type_library("windows-64-7sp1");
    ASSERT_TRUE(tlib != nullptr) << "Could not locate requested profile";

    auto st = translate(tlib, "_EPROCESS");
    ASSERT_TRUE(is_valid_structure_type(st)) << "Could not locate _EPROCESS type";

    auto mr = offset_of(tlib, st, "Win32Process");
    ASSERT_TRUE(mr->offset == 0x258) << "Invalid offset found";
    free_member_result(mr);
}
