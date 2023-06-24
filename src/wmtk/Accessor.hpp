#pragma once

#include <type_traits>
#include "AttributeHandle.hpp"
#include "Tuple.hpp"

#include <Eigen/Dense>

namespace wmtk {
class Mesh;
class TriMesh;
enum class AccessorWriteMode {
    Immediate,
    Buffered
}

template <typename T>
class MeshAttributes;
template <typename T>
class AccessorWriteCache<T>
template <typename T, bool IsConst = false>
class Accessor
{
public:
    friend class Mesh;
    friend class TriMesh;
    using MeshType = std::conditional_t<IsConst, const Mesh, Mesh>;
    using MeshAttributesType = std::conditional_t<IsConst, const Mesh, Mesh>;

    using MapResult = typename Eigen::Matrix<T, Eigen::Dynamic, 1>::MapType;
    using ConstMapResult = typename Eigen::Matrix<T, Eigen::Dynamic, 1>::ConstMapType;


    Accessor(MeshType& m, const MeshAttributeHandle<T>& handle, AccessorWriteMode mode = AccessorWriteMode::Immediate);

    ConstMapResult vector_attribute(const Tuple& t) const;
    MapResult vector_attribute(const Tuple& t);

    T scalar_attribute(const Tuple& t) const;
    T& scalar_attribute(const Tuple& t);

    ConstMapResult vector_attribute(const long index) const;
    MapResult vector_attribute(const long index);

    T scalar_attribute(const long index) const;
    T& scalar_attribute(const long index);

private:
    MeshAttributes<T>& attributes();
    const MeshAttributes<T>& attributes() const;

    MeshType& m_mesh;
    MeshAttributeHandle<T> m_handle;

    AccessorWriteMode m_write_mode = AccessorWriteMode::Immediate;
    std::unique_ptr<AccessorWriteCache> m_write_cache;
};

// template <typename T>
// Accessor(Mesh& mesh, const MeshAttributeHandle<T>&) -> Accessor<T>;
// template <typename T>
// Accessor(const Mesh& mesh, const MeshAttributeHandle<T>&) -> Accessor<const T>;


template <typename T>
using ConstAccessor = Accessor<T, true>;
} // namespace wmtk
