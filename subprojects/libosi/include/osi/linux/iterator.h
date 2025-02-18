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
// base class iterator for objects connected by list_heads
class base_iterator
{
protected:
    i_t object;
    std::string member;
    uint64_t offset;

public:
    base_iterator(i_t& obj, const std::string& m)
    {
        object = obj;
        member = m;
        struct MemberResult* mem =
            offset_of(obj.get_type_library(), obj.get_type(), m.c_str());
        offset = mem->offset;
    }

    i_t& get() { return object; }

    base_iterator& prev()
    {
        i_t prev = object[member]("prev");
        object.set_address(prev.get_address() - offset);
        return *this;
    }

    uint64_t get_offset() { return offset; }
    bool has_next()
    {
        if (object[member].get_address() != 0) {
            auto next = object[member]("next");
            try {
                // TODO use virtual memory handle to check if valid
                next.getu();
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    base_iterator& next()
    {
        i_t next = object[member]("next");
        object.set_address(next.get_address() - offset);
        return *this;
    }

    i_t& operator*() { return get(); }

    base_iterator& operator--(int) { return prev(); }

    base_iterator& operator++(int) { return next(); }

    bool operator==(base_iterator& that) const { return object == that.get(); }

    bool operator!=(base_iterator& that) const { return !(*this == that); }
};

/*
 *  Iterator for going task_struct -> task_struct via list_head structs
 *    (1) process->process->process... via tasks field
 *    (2) process->thread->thread...   via thread_group field
 */
class task_iterator : public base_iterator
{
private:
    uint64_t original_pid;

public:
    task_iterator(i_t& obj, const std::string& m) : base_iterator(obj, m)
    {
        original_pid = obj["pid"].get32();
    }

    bool is_init_task()
    {
        if (object["pid"].get32() == 0) {
            return true;
        }
        return false;
    }

    bool is_original_task()
    {
        // for tracking state (have we done a full loop)
        if (object["pid"].get32() == original_pid) {
            return true;
        }
        return false;
    }
};

} // namespace osi

#endif // __OSI_ITERATOR
