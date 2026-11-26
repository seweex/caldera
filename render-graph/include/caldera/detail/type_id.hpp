#pragma once

namespace caldera::detail {
template <class> struct TypeTag {
    static inline char value = 0;
};

template <class Ty> [[nodiscard]] constexpr const void *get_type_id() { return &TypeTag<Ty>::value; }
} // namespace caldera::detail