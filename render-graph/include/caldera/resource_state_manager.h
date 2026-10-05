#pragma once

#include <memory>
#include "fwd.h"

namespace caldera
{
    class ResourceStateManager
    {
    public:
        ResourceStateManager();
        ~ResourceStateManager() noexcept;

        ResourceStateManager(ResourceStateManager &&) noexcept;
        ResourceStateManager& operator=(ResourceStateManager &&) noexcept;

        ResourceStateManager(const ResourceStateManager &) = delete;
        ResourceStateManager& operator =(const ResourceStateManager &) = delete;

        void reset_for(ImageID id) noexcept;
        void reset_for(BufferID id) noexcept;

        struct ExecutorAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
