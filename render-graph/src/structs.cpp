#include <caldera/structs.hpp>
#include <cassert>

namespace caldera {
uint32_t FamilyNumbers::get(FamilyType const type) const noexcept {
    switch (type) {
    case FamilyType::graphics:
        return graphics;
    case FamilyType::transfer:
        return transfer;
    case FamilyType::compute:
        return compute;
    default:
        assert(false);
    }
}
} // namespace caldera
