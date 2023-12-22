#pragma once
#include <wmtk/operations/CollapseNewAttributeStrategy.hpp>
#include "EdgeOperationData.hpp"

namespace wmtk::operations::edge_mesh {
class CollapseNewAttributeStrategy : public wmtk::operations::CollapseNewAttributeStrategy
{
public:
    // NOTE: this constructor doesn't actually use the mesh passed in - this is just to make
    // sure
    // the input mesh is a EdgeMesh / avoid accidental mistakes
    CollapseNewAttributeStrategy(EdgeMesh&);


    EdgeMesh& edge_mesh();
    const EdgeMesh& edge_mesh() const;

private:
    // the sipmlices that were merged together
    std::vector<std::array<Tuple, 2>> merged_simplices(
        const ReturnVariant& ret_data,
        const Tuple& input_tuple,
        PrimitiveType pt) const final override;

    // the simplices that were created by merging simplices
    std::vector<Tuple> new_simplices(
        const ReturnVariant& ret_data,
        const Tuple& input_tuple,
        PrimitiveType pt) const final override;

    // the sipmlices that were merged together
    std::vector<std::array<Tuple, 2>> merged_simplices(
        const EdgeOperationData& ret_data,
        const Tuple& input_tuple,
        PrimitiveType pt) const;

    // the simplices that were created by merging simplices
    std::vector<Tuple> new_simplices(
        const EdgeOperationData& ret_data,
        const Tuple& input_tuple,
        PrimitiveType pt) const;
};
} // namespace wmtk::operations::edge_mesh
