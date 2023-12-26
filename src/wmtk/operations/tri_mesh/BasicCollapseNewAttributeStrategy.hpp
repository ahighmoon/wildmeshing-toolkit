#pragma once
#include "CollapseNewAttributeStrategy.hpp"

namespace wmtk::operations::tri_mesh {

template <typename T>
class BasicCollapseNewAttributeStrategy : public CollapseNewAttributeStrategy
{
public:
    using VecType = VectorX<T>;
    using CollapseFuncType = std::function<VecType(const VecType&, const VecType&)>;
    BasicCollapseNewAttributeStrategy(const wmtk::attribute::MeshAttributeHandle<T>& h);

    void assign_collapsed(
        PrimitiveType pt,
        const std::array<Tuple, 2>& input_simplices,
        const Tuple& final_simplex) override;


    void set_collapse_strategy(CollapseFuncType&& f);
    void set_standard_collapse_strategy(CollapseBasicStrategy t) override;


    Mesh& mesh() override;
    PrimitiveType primitive_type() const override;

    void update_handle_mesh(Mesh& m) override;

private:
    wmtk::attribute::MeshAttributeHandle<T> m_handle;
    CollapseFuncType m_collapse_op;
};


template <typename T>
BasicCollapseNewAttributeStrategy(const wmtk::attribute::MeshAttributeHandle<T>&)
    -> BasicCollapseNewAttributeStrategy<T>;
} // namespace wmtk::operations::tri_mesh
