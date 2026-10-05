#include <caldera/pass_declarator.h>
#include "detail/pass_declarator.h"

#include "caldera/graph_declarator.h"
#include "detail/graph_declarator.h"

namespace caldera
{
    /* Pass Declarator */

    PassDeclarator::PassDeclarator(
        std::string_view const passName,
        FamilyType const family,
        GraphDeclarator &graphDeclarator)
    :
        m_impl(std::make_unique<Impl>(
            graphDeclarator, GraphDeclarator::PassAttorney::emplace_pass(graphDeclarator, passName, family)))
    {}

    PassDeclarator::~PassDeclarator() noexcept = default;

    PassDeclarator::PassDeclarator(PassDeclarator &&) noexcept = default;
    PassDeclarator & PassDeclarator::operator=(PassDeclarator &&) noexcept = default;

    ImageVersion PassDeclarator::read(ImageVersion const resource, ImageUsage const usage) {
        auto& impl = *m_impl;
        return GraphDeclarator::PassAttorney::push_reading(impl.declarator, impl.passIdx, resource, usage);
    }

    ImageVersion PassDeclarator::read(ImageID const resource, ImageUsage const usage) {
        return this->read(ImageVersion{ resource, 0 }, usage);
    }

    BufferVersion PassDeclarator::read(BufferVersion const resource, BufferUsage const usage) {
        auto& impl = *m_impl;
        return GraphDeclarator::PassAttorney::push_reading(impl.declarator, impl.passIdx, resource, usage);
    }

    BufferVersion PassDeclarator::read(BufferID const resource, BufferUsage const usage) {
        return this->read(BufferVersion{ resource, 0 }, usage);
    }

    ImageVersion PassDeclarator::write(ImageVersion const resource, ImageUsage const usage) {
        auto& impl = *m_impl;
        return GraphDeclarator::PassAttorney::push_writing(impl.declarator, impl.passIdx, resource, usage);
    }

    ImageVersion PassDeclarator::write(ImageID const resource, ImageUsage const usage) {
        return this->write(ImageVersion{ resource, 0 }, usage);
    }

    BufferVersion PassDeclarator::write(BufferVersion const resource, BufferUsage const usage) {
        auto& impl = *m_impl;
        return GraphDeclarator::PassAttorney::push_writing(impl.declarator, impl.passIdx, resource, usage);
    }

    BufferVersion PassDeclarator::write(BufferID const resource, BufferUsage const usage) {
        return this->write(BufferVersion{ resource, 0 }, usage);
    }

    ImageVersion PassDeclarator::read_write(ImageVersion const resource, ImageUsage const usage) {
        auto& impl = *m_impl;
        auto read = GraphDeclarator::PassAttorney::push_reading(impl.declarator, impl.passIdx, resource, usage);
        return GraphDeclarator::PassAttorney::push_writing(impl.declarator, impl.passIdx, read, usage);
    }

    ImageVersion PassDeclarator::read_write(ImageID const resource, ImageUsage const usage) {
        return this->read_write(ImageVersion{ resource, 0 }, usage);
    }

    BufferVersion PassDeclarator::read_write(BufferVersion const resource, BufferUsage const usage) {
        auto& impl = *m_impl;
        auto read = GraphDeclarator::PassAttorney::push_writing(impl.declarator, impl.passIdx, resource, usage);
        return GraphDeclarator::PassAttorney::push_writing(impl.declarator, impl.passIdx, read, usage);
    }

    BufferVersion PassDeclarator::read_write(BufferID const resource, BufferUsage const usage) {
        return this->read_write(BufferVersion{ resource, 0 }, usage);
    }

    void PassDeclarator::set_callback(
        std::function<void(ResourceMap const &, vk::CommandBuffer)> callback)
    {
        assert(callback != nullptr);

        auto& impl = *m_impl;
        GraphDeclarator::PassAttorney::set_pass_callback(impl.declarator, impl.passIdx, std::move(callback));
    }
}
