#ifndef __OSI_ITERATOR
#define __OSI_ITERATOR

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include <iohal/memory/physical_memory.h>
#include <iohal/memory/virtual_memory.h>
#include <offset/i_t.h>
#include <offset/offset.h>

namespace osi
{
class iterator
{
private:
    i_t object;
    std::string member;
    uint64_t tid_offset;
    uint64_t __attribute__((unused)) max_iterations; /* Is there any way someone could be depending on this that precludes removal? */

public:
    iterator(i_t& obj, const std::string& m)
    {
        object = obj;
        member = m;
        struct MemberResult* mem =
            offset_of(obj.get_type_library(), obj.get_type(), m.c_str());

        tid_offset = mem->offset;
        free_member_result(mem);
    }

    i_t& get() { return object; }

    iterator& prev()
    {
        i_t blink = object[member]("Blink");
        object.set_address(blink.get_address() - tid_offset);
        return *this;
    }

    uint64_t get_offset() { return tid_offset; }

    bool has_next()
    {
        if (object[member].get_address() != 0) {
            auto flink = object[member]("Flink");
            try {
                // TODO use virtual memory handle to check if valid
                flink.getu();
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    iterator& next()
    {
        i_t blink = object[member]("Flink");
        object.set_address(blink.get_address() - tid_offset);
        return *this;
    }

    i_t& operator*() { return get(); }

    iterator& operator--(int) { return prev(); }

    iterator& operator++(int) { return next(); }

    bool operator==(iterator& that) const { return object == that.get(); }

    bool operator!=(iterator& that) const { return !(*this == that); }
};

class iterator32
{
private:
    i_t object;
    std::string member;
    uint64_t tid_offset;
    uint64_t __attribute__((unused)) max_iterations; /* Is there any way someone could be depending on this that precludes removal? */

public:
    iterator32(i_t& obj, const std::string& m)
    {
        object = obj;
        member = m;
        struct MemberResult* mem =
            offset_of(obj.get_type_library(), obj.get_type(), m.c_str());

        tid_offset = mem->offset;
        free_member_result(mem);
    }

    i_t& get() { return object; }

    iterator32& prev()
    {
        uint32_t blink_address = object[member]["Blink"].get32();
        i_t blink = i_t(object.get_virtual_memory_shared(), object.get_type_library(),
                        blink_address, "LIST_ENTRY32");
        object.set_address(blink.get_address() - tid_offset);
        return *this;
    }

    uint64_t get_offset() { return tid_offset; }

    bool has_next()
    {
        if (object[member].get_address() != 0) {
            uint32_t flink_address = object[member]["Flink"].get32();
            auto flink = i_t(object.get_virtual_memory_shared(),
                             object.get_type_library(), flink_address, "LIST_ENTRY32");
            try {
                // TODO use virtual memory handle to check if valid
                flink.getu();
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    iterator32& next()
    {
        uint32_t blink_address = object[member]["Flink"].get32();
        i_t blink = i_t(object.get_virtual_memory_shared(), object.get_type_library(),
                        blink_address, "LIST_ENTRY32");
        object.set_address(blink.get_address() - tid_offset);
        return *this;
    }

    i_t& operator*() { return get(); }

    iterator32& operator--(int) { return prev(); }

    iterator32& operator++(int) { return next(); }

    bool operator==(iterator32& that) const { return object == that.get(); }

    bool operator!=(iterator32& that) const { return !(*this == that); }
};
} // namespace osi

#endif // __OSI_ITERATOR
