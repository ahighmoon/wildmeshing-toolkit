#pragma once
#include <spdlog/spdlog.h>
#include <tuple>
#include "Simplex.hpp"
#include "Tuple.hpp"
#include "attribute/AttributeManager.hpp"
#include "attribute/AttributeScopeHandle.hpp"
#include "attribute/MeshAttributes.hpp"

namespace wmtk {
class Mesh;
class MultiMeshManager
{
public:
    MultiMeshManager();
    ~MultiMeshManager();
    MultiMeshManager(const MultiMeshManager& o);
    MultiMeshManager(MultiMeshManager&& o);
    MultiMeshManager& operator=(const MultiMeshManager& o);
    MultiMeshManager& operator=(MultiMeshManager&& o);

    //=========================================================
    // Storage of MultiMesh
    //=========================================================

    bool is_root() const;
    long child_id() const;
    //
    std::vector<long> absolute_id() const;


    // register child_meshes and the map from child_meshes to this mesh, child_mesh_simplex
    void register_child_mesh(
        Mesh& my_mesh,
        const std::shared_ptr<Mesh>& child_mesh,
        const std::vector<std::array<Tuple, 2>>& child_mesh_simplex_map);


    bool are_maps_valid(const Mesh& my_mesh) const;

    //===========
    //===========
    // Map functions
    //===========
    //===========
    // Note that when we map a M-tuplefrom a K-complex to a J-complex there are different
    // relationships necessary if K == J
    //    if M == K then this is unique
    //    if M < K then this is many to many
    // if K < J
    //    if M == K then it is one to many
    //    if M < K then it is many to many

    //===========
    // Simplex maps
    //===========
    // generic mapping function that maps a tuple from "this" mesh to the other mesh
    std::vector<Simplex> map(const Mesh& my_mesh, const Mesh& other_mesh, const Simplex& my_simplex)
        const;


    Tuple map_to_parent(const Mesh& my_mesh, const Simplex& my_simplex) const;
    Tuple map_to_parent_tuple(const Mesh& my_mesh, const Simplex& my_simplex) const;

    // generic mapping function that maps a tuple from "this" mesh to one of its children
    std::vector<Tuple>
    map_to_child(const Mesh& my_mesh, const Mesh& child_mesh, const Simplex& my_simplex) const;


    // Utility function to map a edge tuple to all its children, used in operations
    std::vector<Tuple> map_edge_tuple_to_all_children(const Mesh& my_mesh, const Simplex& tuple)
        const;

protected: // protected to enable unit testing
    //===========
    // Tuple maps
    //===========
    // generic mapping function that maps a tuple from "this" mesh to the other mesh
    std::vector<Tuple>
    map_to_tuples(const Mesh& my_mesh, const Mesh& other_mesh, const Tuple& my_tuple) const;

    // generic mapping function that maps a tuple from "this" mesh to its parent
    Tuple map_tuple_to_parent(const Mesh& my_mesh, const Mesh& parent_mesh, const Tuple& my_tuple)
        const;

    // generic mapping function that maps a tuple from "this" mesh to one of its children
    Tuple map_to_child(const Mesh& my_mesh, const Mesh& child_mesh, const Tuple& my_tuple) const;

    // generic mapping function that maps a tuple from "this" mesh to one of its children (by index)
    Tuple map_to_child(const Mesh& my_mesh, long child_id, const Tuple& my_tuple) const;

    // wrapper for implementing converting tuple to a child using the internal map data
    std::vector<Tuple>
    map_to_child(const Mesh& my_mesh, const ChildData& child_data, const Tuple& simplex) const;

    std::vector<Tuple> convert_single_tuple(
        const Mesh& my_mesh,
        const ChildData& child_data,
        const Simplex& simplex) const;

    // helper function to check if this mesh is a valid child_mesh of my_mesh
    // i.e. the connectivity of this mesh is a subset of this in my_mesh
    bool is_child_mesh_valid(const Mesh& my_mesh, const Mesh& child_mesh) const;

    // checks that the map is consistent
    bool is_child_map_valid(const Mesh& my_mesh, const ChildData& child) const;


    static Tuple map_tuple_between_meshes(
        const Mesh& source_mesh,
        const Mesh& target_mesh,
        const ConstAccessor<long>& source_to_target_map_accessor,
        const Tuple& source_tuple);

private:
    Mesh* m_parent = nullptr;
    // only valid if this is teh child of some other mesh
    // store the map to the base_tuple of the my_mesh
    MeshAttributeHandle<long> map_to_parent_handle;
    long m_child_id = -1;

    struct ChildData
    {
        std::shared_ptr<Mesh> mesh;
        // store the map from the manager's mesh to the child mesh (on the top
        // level simplex of the mesh)
        // encoded by a pair of two tuples, from a tuple in current mesh to a tuple in
        // child_mesh
        MeshAttributeHandle<long> map_handle;
    };

    // Child Meshes
    std::vector<ChildData> m_children;
};

} // namespace wmtk
