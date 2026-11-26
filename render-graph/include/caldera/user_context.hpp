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
/**
 * @brief Non-owning type-indexed container of references to user-provided objects.
 * @note The context stores only addresses; every assigned object must outlive the context
 *       (or be re-assigned before it is destroyed).
 * @note Types are matched without RTTI, ignoring cv-qualifiers and references.
 *       Type ids are not guaranteed to be consistent across shared library boundaries.
 * @note ResourceMap, ResourceStateManager and CommandBufferDistributor are stored in dedicated
 *       slots for constant-time access; all other types are looked up linearly, which is fast
 *       for a small number of entries.
 * @note Not thread-safe: concurrent assign() calls, or assign() concurrent with any lookup,
 *       require external synchronization. Concurrent lookups alone are safe.
 */
class UserContext {
    struct DataRef {
        void const *typeID;
        void *data;
    };

    template <class Ty>
    static constexpr bool is_hooked = std::same_as<std::remove_cvref_t<Ty>, ResourceMap> ||
                                      std::same_as<std::remove_cvref_t<Ty>, ResourceStateManager> ||
                                      std::same_as<std::remove_cvref_t<Ty>, CommandBufferDistributor>;

    template <class Ty> [[nodiscard]] std::remove_cvref_t<Ty> *&hook() noexcept {
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
    /**
     * @brief Registers a non-owning reference to an object in the context.
     * @param value Object to register. Must outlive this context (or until it is re-assigned).
     * @note The type must be specified explicitly: assign<Foo>(foo).
     * @note The context does not take ownership; only the address is stored.
     * @note A repeated call replaces the previous reference
     */
    template <class Ty> void assign(std::remove_cvref_t<Ty> &value) {
        auto const ptr = std::addressof(value);

        if constexpr (is_hooked<Ty>)
            hook<Ty>() = ptr;
        else {
            auto constexpr id = get_id<Ty>();
            auto const iter = std::ranges::find(m_refs, id, &DataRef::typeID);

            if (iter == m_refs.end())
                m_refs.emplace_back(get_id<Ty>(), ptr);
            else
                iter->data = ptr;
        }
    }

    /**
     * @brief Looks up a registered object by type.
     * @return Pointer to the object, or nullptr if an object of this type was not assigned.
     * @note Cv-qualifiers and references on Ty are ignored when matching the type.
     */
    template <class Ty> [[nodiscard]] Ty *try_get() noexcept {
        if constexpr (is_hooked<Ty>)
            return hook<Ty>();
        else {
            if (auto const iter = std::ranges::find(m_refs, get_id<Ty>(), &DataRef::typeID); iter != m_refs.end())
                return static_cast<Ty *>(iter->data);

            return nullptr;
        }
    }

    /** @copydoc try_get() */
    template <class Ty> [[nodiscard]] Ty const *try_get() const noexcept {
        return const_cast<UserContext *>(this)->try_get<Ty>();
    }

    /**
     * @brief Returns a reference to a registered object by type.
     * @return Reference to the object.
     * @note The object must have been assigned beforehand. Otherwise the behavior is undefined
     *       (an assertion fires in debug builds). Use try_get() if presence is not guaranteed.
     */
    template <class Ty> [[nodiscard]] Ty &get() {
        if (auto const ptr = try_get<Ty>())
            return *ptr;

        assert(false && "value of the type wasn't found");
        std::unreachable();
    }

    /** @copydoc get() */
    template <class Ty> [[nodiscard]] Ty const &get() const { return const_cast<UserContext *>(this)->get<Ty>(); }

    /**
     * @brief Checks whether all specific objects have been assigned.
     * @return true if ResourceMap, ResourceStateManager and CommandBufferDistributor are all set.
     * @note Only contexts with all specific objects can be executed.
     */
    [[nodiscard]] bool has_specifics() const noexcept { return m_resourceMap && m_stateManager && m_cmdDistributor; }

  private:
    [[nodiscard]] std::tuple<ResourceMap &, ResourceStateManager &, CommandBufferDistributor &>
    get_specifics() noexcept {
        assert(has_specifics());
        return std::forward_as_tuple(*m_resourceMap, *m_stateManager, *m_cmdDistributor);
    }

    friend class GraphExecutor;

    ResourceMap *m_resourceMap = nullptr;
    ResourceStateManager *m_stateManager = nullptr;
    CommandBufferDistributor *m_cmdDistributor = nullptr;

    std::vector<DataRef> m_refs;
};
} // namespace caldera