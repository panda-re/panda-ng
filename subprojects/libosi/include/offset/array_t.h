#ifndef __OSI_ARRAY_T
#define __OSI_ARRAY_T

#include "i_t.h"
#include "offset.h"

namespace osi
{
// represents an array of pointers to osi::i_t types
class array_t
{
private:
    i_t m_ptr_obj;
    const StructureType* m_type;
    vm_addr_t m_address;

public:
    array_t(i_t obj)
    {
        auto ptr_type = obj.get_type();

        if (!is_pointer_structure_type(ptr_type))
            throw std::runtime_error("invalid array_t: not a POINTER");

        m_ptr_obj = obj;
        m_type = dereference_st(obj.get_type_library(), ptr_type);
        m_address = obj.getu();
    }

    i_t get_element(uint64_t idx)
    {
        auto vmem = m_ptr_obj.get_virtual_memory_shared();

        i_t element =
            m_ptr_obj.set_address(m_address + (vmem->get_pointer_width() * idx));
        return i_t(vmem, m_ptr_obj.get_type_library(), element.getu(), m_type);
    }
};
} // namespace osi

#endif
