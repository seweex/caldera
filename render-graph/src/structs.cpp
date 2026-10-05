
#include "detail/structs.h"

namespace caldera
{
    uint32_t FamilyNumbers::get(FamilyType const type) const noexcept
    {
        switch (type)
        {
        case FamilyType::graphics: return graphics;
        case FamilyType::transfer: return transfer;
        case FamilyType::compute:  return compute;
        default:                   return vk::QueueFamilyIgnored;
        }
    }
}

namespace caldera::detail
{
    bool ImageVersionDesc::Toucher::Comparator::operator()(
        Toucher const &l, Toucher const &r) const noexcept
    {
        if (l.usage == r.usage)
            return l.family < r.family;

        return l.usage < r.usage;
    }

    bool BufferVersionDesc::Toucher::Comparator::operator()(
        Toucher const &l, Toucher const &r) const noexcept
    {
        if (l.usage == r.usage)
            return l.family < r.family;

        return l.usage < r.usage;
    }
}
