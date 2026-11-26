#pragma once

#include <algorithm>
#include <cassert>
#include <utility>
#include <memory>
#include <ranges>
#include <type_traits>
#include <vector>

#include <caldera/detail/type_id.hpp>

namespace caldera {
class UserContext {
    struct DataRef {
        void const *typeID;
        void *data;
    };

    template <class Ty> static consteval void const *get_id() noexcept {
        return detail::get_type_id<std::remove_cvref_t<Ty>>();
    }

  public:
    template <class Ty> void push(std::remove_cvref_t<Ty> &value) {
        m_refs.emplace_back(get_id<Ty>(), std::addressof(value));
    }

    template <class Ty> [[nodiscard]] Ty *try_get() noexcept {
        if (auto const iter = std::ranges::find(m_refs, get_id<Ty>(), &DataRef::typeID); iter != m_refs.end())
            return static_cast<Ty *>(iter->data);

        return nullptr;
    }

    template <class Ty> [[nodiscard]] Ty const *try_get() const noexcept {
        return const_cast<UserContext *>(this)->try_get<Ty>();
    }

    template <class Ty> [[nodiscard]] Ty &get() {
        if (auto const ptr = try_get<Ty>())
            return *ptr;

        assert(!"value of the type wasn't found");
        std::unreachable();
    }

    template <class Ty> [[nodiscard]] Ty const &get() const { return const_cast<UserContext *>(this)->get<Ty>(); }

  private:
    std::vector<DataRef> m_refs;
};
} // namespace caldera