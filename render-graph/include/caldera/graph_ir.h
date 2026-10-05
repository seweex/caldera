#pragma once

#include <memory>

namespace caldera
{
    class GraphIR
    {
        GraphIR();

    public:
        ~GraphIR() noexcept;

        GraphIR(GraphIR&&) noexcept;
        GraphIR& operator=(GraphIR&&) noexcept;

        GraphIR(GraphIR const&) = delete;
        GraphIR& operator=(GraphIR const&) = delete;

        struct Factory;
        struct ResourcesAttorney;
        struct ExecutionAttorney;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
