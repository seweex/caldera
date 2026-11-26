#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <memory>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <caldera/detail/type_id.hpp>
#include <caldera/fwd.hpp>

namespace caldera {
class UserContext {
    struct DataRef {
        void const *typeID;
        void *data;
    };

    template <class Ty>
    static constexpr bool is_hooked = std::same_as<std::remove_cvref_t<Ty>, ResourceMap> ||
                                      std::same_as<std::remove_cvref_t<Ty>, ResourceStateManager> ||
                                      std::same_as<std::remove_cvref_t<Ty>, CommandBufferDistributor>;

    template <class Ty> [[nodiscard]] std::remove_cvref_t<Ty> *&hook() noexcept
    {
        using Type = std::remove_cvref_t<Ty>;
        static_assert(is_hooked<Type>);

        if constexpr (std::same_as<Type, ResourceMap>)
            return m_resourceMap;
        else if constexpr (std::same_as<Type, ResourceStateManager>)
            return m_stateManager;
        else
            return m_cmdDistributor;
    }

    template <class Ty> static consteval void const *get_id() noexcept {
        return detail::get_type_id<std::remove_cvref_t<Ty>>();
    }

  public:
    template <class Ty> void assign(std::remove_cvref_t<Ty> &value) {
        if constexpr (is_hooked<Ty>)
            hook<Ty>() = std::addressof(value);
        else {
            m_refs.emplace_back(get_id<Ty>(), std::addressof(value));
        }
    }

    template <class Ty> [[nodiscard]] Ty *try_get() noexcept {
        if constexpr (is_hooked<Ty>)
            return hook<Ty>();
        else {
            if (auto const iter = std::ranges::find(m_refs, get_id<Ty>(), &DataRef::typeID); iter != m_refs.end())
                return static_cast<Ty *>(iter->data);

            return nullptr;
        }
    }

    template <class Ty> [[nodiscard]] Ty const *try_get() const noexcept {
        return const_cast<UserContext *>(this)->try_get<Ty>();
    }

    template <class Ty> [[nodiscard]] Ty &get() {
        if (auto const ptr = try_get<Ty>())
            return *ptr;

        assert(false && "value of the type wasn't found");
        std::unreachable();
    }

    template <class Ty> [[nodiscard]] Ty const &get() const { return const_cast<UserContext *>(this)->get<Ty>(); }

    [[nodiscard]] bool has_specifics() const noexcept { return m_resourceMap && m_stateManager && m_cmdDistributor; }

  private:
    [[nodiscard]] std::tuple<ResourceMap &, ResourceStateManager &, CommandBufferDistributor &>
    get_specifics() noexcept {
        assert(has_specifics());
        return std::forward_as_tuple(*m_resourceMap, *m_stateManager, *m_cmdDistributor);
    }

    ResourceMap *m_resourceMap = nullptr;
    ResourceStateManager *m_stateManager = nullptr;
    CommandBufferDistributor *m_cmdDistributor = nullptr;

    std::vector<DataRef> m_refs;
};
} // namespace caldera