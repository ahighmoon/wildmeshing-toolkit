#pragma once
#include <type_traits>
#include <vector>
#include <wmtk/Tuple.hpp>

namespace wmtk {
class Mesh;
class Operation
{
public:
    // main entry point of the operator by the scheduler
    bool operator()();
    virtual std::string name() const = 0;


    Operation(Mesh& m);
    virtual ~Operation();

    virtual std::vector<double> priority() const { return {0}; }
    virtual std::vector<Tuple> modified_triangles() const = 0;

protected:
    virtual bool execute() = 0;
    virtual bool before() const = 0;
    virtual bool after() const;

    Mesh& m_mesh;
};


} // namespace wmtk

