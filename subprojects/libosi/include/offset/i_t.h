#ifndef __OFFSET_I_T
#define __OFFSET_I_T

#include <codecvt>
#include <cuchar>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "offset.h"
#include <iohal/memory/physical_memory.h>
#include <iohal/memory/virtual_memory.h>

#define POINTER 0x80000000

namespace osi
{
class i_t
{
private:
    std::shared_ptr<VirtualMemory> m_vmem;
    StructureTypeLibrary* m_tlib;
    vm_addr_t m_address;
    const struct StructureType* m_type;

public:
    i_t() : m_vmem(nullptr), m_tlib(nullptr), m_address(0), m_type(0){};

    i_t(std::shared_ptr<VirtualMemory> vmem, struct StructureTypeLibrary* tlib,
        vm_addr_t a, const struct StructureType* type)
        : m_vmem(vmem), m_tlib(tlib), m_address(a), m_type(type){};

    i_t(std::shared_ptr<VirtualMemory> vmem, struct StructureTypeLibrary* tlib,
        vm_addr_t a, std::string t)
        : m_vmem(vmem), m_tlib(tlib), m_address(a)
    {
        m_type = translate(tlib, t.c_str());
    };

    struct StructureTypeLibrary* get_type_library() { return m_tlib; }

    const struct StructureType* get_type() const { return m_type; }

    VirtualMemory* get_virtual_memory() { return m_vmem.get(); }

    std::shared_ptr<VirtualMemory> get_virtual_memory_shared() { return m_vmem; }

    vm_addr_t get_address() const { return m_address; }

    i_t get_member(const std::string& m) const
    {
        if (is_unknown_structure_type(m_type)) {
            throw std::runtime_error("cannot get member of UNKNOWN type (cast me?)");
        }
        auto member = offset_of(m_tlib, m_type, m.c_str());
        if (member->offset == INVALID_OFFSET) {
            throw std::runtime_error("Invalid member " + m);
        }
        auto retval = i_t(m_vmem, m_tlib, m_address + member->offset, member->type);
        free_member_result(member);
        return retval;
    }

    i_t& set_type(const std::string& t)
    {
        m_type = translate(m_tlib, t.c_str());
        return *this;
    }

    i_t& set_type(const struct StructureType* t)
    {
        m_type = t;
        return *this;
    }

    i_t& set_address(vm_addr_t a)
    {
        m_address = a;
        return *this;
    }

    bool is_pointer() const { return is_pointer_structure_type(m_type); }

    i_t dereference() const
    {
        if (!is_pointer()) {
            throw std::runtime_error("invalid dereference: not a pointer");
        }
        vm_addr_t new_addr = 0;
        auto status = m_vmem->read_pointer(m_address, &new_addr);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid dereference: vmem->read failed");
        }
        return i_t(m_vmem, m_tlib, new_addr, dereference_st(m_tlib, m_type));
    }

    i_t operator[](const std::string& m) const { return get_member(m); }

    i_t operator()(const std::string& m) const { return get_member(m).dereference(); }

    i_t operator*() const { return dereference(); }

    vm_addr_t operator&() const { return get_address(); }

    bool operator==(const i_t& that) const
    {
        return get_address() == that.get_address() &&
               equal_structure_types(get_type(), that.get_type());
    }

    bool operator!=(const i_t& that) const { return !(*this == that); }

    template <typename T = uint8_t> T get8() const
    {
        T value;
        auto status = m_vmem->read(m_address, (uint8_t*)&value, 1);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid read get8: vmem->read failed");
        }
        return value;
    }

    template <typename T = uint16_t> T get16() const
    {
        T value;
        auto status = m_vmem->read(m_address, (uint8_t*)&value, 2);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid read get16: vmem->read failed");
        }
        return value;
    }

    template <typename T = uint32_t> T get32() const
    {
        T value;
        auto status = m_vmem->read(m_address, (uint8_t*)&value, 4);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid read get32: vmem->read failed");
        }
        return value;
    }

    template <typename T = uint64_t> T get64() const
    {
        T value;
        auto status = m_vmem->read(m_address, (uint8_t*)&value, 8);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid read get64: vmem->read failed");
        }
        return value;
    }

    template <typename T = vm_addr_t> T getu() const
    {
        T value;
        auto status = m_vmem->read_pointer(m_address, &value);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid read getu: vmem->read failed");
        }
        return value;
    }

    template <typename T> T& getx(T& value, const int length) const
    {
        auto status = m_vmem->read(m_address, &value, length);
        if (status != TSTAT_SUCCESS) {
            throw std::runtime_error("invalid read getx: vmem->read failed");
        }
        return value;
    }

    std::string get_wchar_str(int length) const
    {
        if (length <= 0 || m_address == 0 || length > 2048) {
            return std::string("");
        }

        if (length % 2) {
            fprintf(stderr, "Warning: Asked to parse wide string of odd length\n");
            length -= 1;
        }

        std::unique_ptr<char16_t[]> in_buf_p(new char16_t[length + 2]());
        auto tstat = m_vmem->read(m_address, (char16_t*)in_buf_p.get(), length);

        if (tstat != TSTAT_SUCCESS) {
            return std::string("");
        }

        try {
            std::u16string s(in_buf_p.get(), in_buf_p.get() + length);
            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}
                .to_bytes(s);
        } catch (std::range_error& e) {
            // conversion error raises range_error
        }
        return std::string("");
    }
};
} // namespace osi

#endif
