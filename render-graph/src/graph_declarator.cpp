#include <caldera/resource_id.hpp>
#include <caldera/graph_declarator.hpp>

namespace
{
    template <class TagTy>
    [[nodiscard]] caldera::BasicResourceID<TagTy> make_transient(
        auto& list,
        auto const& description)
    {
        auto const resourceIdx = static_cast<uint32_t>(list.size());
        list.emplace_back(description);

        return caldera::BasicResourceID<TagTy>{
            .isTransient = 1,
            .index = resourceIdx
        };
    }
}

namespace caldera
{
    ImageID GraphDeclarator::transient_image(ImageDescription const &description) {
        return ::make_transient<ImageTag>(m_ir.transientImages, description);
    }

    BufferID GraphDeclarator::transient_buffer(BufferDescription const &description) {
        return ::make_transient<BufferTag>(m_ir.transientBuffers, description);
    }

    GraphIR GraphDeclarator::extract() const & {
        return m_ir;
    }

    GraphIR GraphDeclarator::extract() && noexcept {
        return std::move(m_ir);
    }
}
