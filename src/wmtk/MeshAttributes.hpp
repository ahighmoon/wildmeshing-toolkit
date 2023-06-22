#pragma once

#include <Eigen/Core>

#include <map>
#include <vector>

namespace wmtk {

class AttributeHandle
{
public:
    long index;
    long stride;
};

template <typename T>
class Transaction
{
public:
    void apply();

private:
    MeshAttributes& m_attribute;
};

template <typename T>
class MeshAttributes
{
    friend class Transaction<T>;

    typedef Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, 1>> MapResult;
    typedef Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, 1>> ConstMapResult;

    MeshAttributes();

    void register_attribute(const std::string& name, long size);

    Accessor<T> register_attribute_with_accessor(const std::string& name, long size);


    long size() const;
    void resize(const long size);

    Accessor<T> create_accessor(const AttributeHandle&);

protected:
    AttributeHandle attribute_handle(const std::string& name) const;

    ConstMapResult vector_attribute(const AttributeHandle& handle, const long index) const;
    MapResult vector_attribute(const AttributeHandle& handle, const long index);

    T scalar_attribute(const AttributeHandle& handle, const long index) const;
    T& scalar_attribute(const AttributeHandle& handle, const long index);

private:
    std::map<std::string, AttributeHandle> m_handles;
    long initial_stride = -1;

    std::vector<std::vector<T>> m_attributes;
};


} // namespace wmtk
#include "MeshAttributes_impl.hpp"
