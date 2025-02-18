#include "offset/offset.h"
#include "osi/windows/wintrospection.h"
#include "gtest/gtest.h"

TEST(WintroBasicTest, Ctors)
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
