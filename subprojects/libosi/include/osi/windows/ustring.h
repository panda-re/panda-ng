#ifndef __OSI_USTRING
#define __OSI_USTRING

#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>
#include <uchar.h>
#include <inttypes.h>

#include <iohal/memory/virtual_memory.h>
#include <offset/i_t.h>
#include <offset/offset.h>

#define MAX_STRING_LEN 4096

namespace osi
{
class ustring
{
private:
    i_t unicode_string;

    std::string read_utf16_string_as_utf8(uint64_t addr, uint64_t nbytes,
                                          bool second_try = false)
    {
        if (addr == 0 || nbytes <= 1) {
            return std::string("");
        }

        if (nbytes > MAX_STRING_LEN) {
            fprintf(stderr, "read_utf16_as_utf8 was asked to parse a string of len %" PRIu64 "\n",
                    nbytes);
            return std::string("");
        }
        if ((nbytes % 2) == 1) {
            fprintf(stderr, "Warning! UTF16 with odd length: %" PRIu64 " at %" PRIx64 "\n",
                    nbytes, addr);
            nbytes -= 1;
        }

        std::unique_ptr<char16_t[]> in_buf_p(new char16_t[nbytes]());
        auto vmem = unicode_string.get_virtual_memory();
        auto tstat = vmem->read(addr, (char16_t*)in_buf_p.get(), nbytes);

        if (tstat != TSTAT_SUCCESS) {
            unsigned jx = 0;
            for (jx = 0; jx < nbytes; ++jx) {
                if (in_buf_p[jx] == 0) {
                    break;
                }
            }

            // This string starts with a null byte - bail
            // this is a little unsafe, since if we can't read the memory
            // at all, we won't have a way to tell
            if (jx == 0) {
                return "";
            }

            // If a partial read would give us a valid string, give it a try.
            // But to prevent a bunch of loops, try exactly once
            if ((nbytes != jx * 2) && !second_try) {
                return read_utf16_string_as_utf8(addr, jx * 2, true);
            }
            return "";
        }

        std::u16string s(in_buf_p.get(), in_buf_p.get() + nbytes);
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        std::string result = convert.to_bytes(s);

        return result;
    }

public:
    ustring(i_t obj)
    {
        if (obj.get_type() != translate(obj.get_type_library(), "_UNICODE_STRING") &&
            obj.get_type() != translate(obj.get_type_library(), "_UNICODE_STRING32")) {
            throw std::runtime_error(
                "invalid ustring: not a _UNICODE_STRING or _UNICODE_STRING32");
        }
        unicode_string = obj;
    }

    uint16_t get_length() { return unicode_string["Length"].get16(); }

    uint16_t get_maximum_length() { return unicode_string["MaximumLength"].get16(); }

    std::string as_utf8()
    {
        uint16_t max_length = get_maximum_length();
        uint16_t length = get_length(); // Length is specified in bytes
        // Check is redundant but want to keep the sentinel checking around for debugging
        if ((length == 0) || (length == 0 && max_length == 1)) {
            return ""; // Sentinel
        }
        if (unicode_string.get_type() ==
            translate(unicode_string.get_type_library(), "_UNICODE_STRING"))
            return read_utf16_string_as_utf8(unicode_string("Buffer").get_address(),
                                             length);

        uint32_t bufferAddress = unicode_string["Buffer"].get32();
        return read_utf16_string_as_utf8(bufferAddress, length);
    };
};
} // namespace osi

#endif // __OSI_USTRING
