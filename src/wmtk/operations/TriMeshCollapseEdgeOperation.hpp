
#pragma once
#include <optional>
#include "Operation.hpp"

namespace wmtk {
class TriMeshCollapseEdgeOperation : public Operation
{
public:
    TriMeshCollapseEdgeOperation(Mesh& m, const Tuple& t);

    std::string name() const override;


    std::vector<Tuple> modified_triangles() const;

    // return a ccw tuple from left ear if it exists, otherwise return a ccw tuple from right ear
    Tuple return_tuple() const;
    // return true if return_tuple() is from left ear, else return false and return_tuple() is from right ear
    bool is_return_tuple_from_left_ear() const;

    static PrimitiveType primitive_type() { return PrimitiveType::Edge; }

protected:
    bool execute() override;
    bool before() const override;

private:
    bool m_is_output_tuple_from_left_ear;
    Tuple m_input_tuple;
    Tuple m_output_tuple;
};


} // namespace wmtk

