#include "VertexSmoothNewtonMethodWithLineSearch.hpp"
#include <wmtk/optimization/LineSearch.hpp>

namespace wmtk::operations::tri_mesh::internal {

VertexSmoothNewtonMethodWithLineSearch::VertexSmoothNewtonMethodWithLineSearch(
    Mesh& m,
    const Tuple& t,
    const OperationSettings<VertexSmoothUsingDifferentiableEnergy>& settings)
    : VertexSmoothNewtonMethod(m, t, settings)
{}

bool VertexSmoothNewtonMethodWithLineSearch::execute()
{
    auto accessor = coordinate_accessor();
    auto interface = get_function_interface<2>(accessor);

    Eigen::Vector2d direction = get_descent_direction(interface);

    optimization::LineSearch<2> line_search(interface, invariants());

    line_search.set_create_scope(
        false); // since we're in an operation we will fail if the seach doesn't do waht we want
    double distance = line_search.run(direction, 1.0);
    if (distance == 0.0) {
        return false;
    }


    if (!tri_mesh::VertexSmoothUsingDifferentiableEnergy::execute()) {
        return false;
    }
    return true;
}


} // namespace wmtk::operations::tri_mesh::internal

