#include <wmtk/AttributeCollectionRecorder.h>
// The actions of an operator are defined by a sequence of commands and how those commands affect
// attributes. They are tehrefore serialized through 2 + N tables of data, where N is the number of
// attributes 1...N) each table containing all attribute updates for a single attribute, organized
// implicitly by attribute and which command
//        each table is named by the attribute name
// N+1) A table of index ranges into the attribute update table, organized by which attribute and
// (implicitly) which command N+2) A ordered table of commands + references to ranges of attribute
// update groups
//
// The attribute update tables use columns [index, old_value, new_value], where
// the index is the index update in some pertinent AttributeCollection and
// the latter two columns are customized for each attribute type.
//
//
// The Index ranges use columns [attribute_name, change_range_begin,change_range_end]
// If we let Data[attribute_name] point to the attribute update table 'attribute_name'
// Then in pythonic-slice notation we see that the updates to 'attribute_name' are stored by
// Data[attribute_name][change_range_begin:change_range_end] .
// This intermediate table is to allow us to track updates of different attributes/types.
//
//
// Finally, the table of commands currently is particular to TriMesh and
// stores [command_name, triangle_id, local_edge_id, vertex_id, update_range_begin,
// update_range_end] where the first 4 entries define a TriMesh command and the latter two specify
// ranges in the previous attribute indexing table to help specify which attributes and which types
// exist.
//


// For any new Attribute type we want to record we have to register the type
// using HighFive's HIGHFIVE_REGISTER_TYPE and then declare its attribute type.
// For example, if we want to record a AttributeCollection<Vec>
// Where Vec is
// struct Vec { double x,y; };
// Then we must declare:
//
// HighFive::CompoundType vec_datatype() {
//      return {
//              {"x", create_datatype<double>()},
//              {"y", create_datatype<double>()}
//      };
// }
//
// and then outside of any namespace's scope we declare the following two lines
// HIGHFIVE_REGISTER_TYPE(Vec, vec_datatype)
// WMTK_HDF5_REGISTER_ATTRIBUTE_TYPE(Vec)
#define WMTK_HDF5_REGISTER_ATTRIBUTE_TYPE(type)              \
    HIGHFIVE_REGISTER_TYPE(                                  \
        wmtk::AttributeCollectionRecorder<type>::UpdateData, \
        wmtk::AttributeCollectionRecorder<type>::datatype)   \
    HIGHFIVE_REGISTER_TYPE(                                  \
        wmtk::AttributeUpdateData<type>, \
        wmtk::AttributeUpdateData<type>::datatype)

#define WMTK_HDF5_DECLARE_ATTRIBUTE_TYPE(type)               \
    HIGHFIVE_REGISTER_TYPE(                                  \
        wmtk::AttributeCollectionRecorder<type>::UpdateData, \
        wmtk::AttributeCollectionRecorder<type>::datatype)

namespace wmtk {
template <typename T>
struct AttributeUpdateData
{
    size_t index;
    T old_value;
    T new_value;
    static HighFive::CompoundType datatype();
};


// Indicates which attribute was changed and the range of updates that are associated with its
// updates in the per-attribute update table
struct AttributeChanges
{
    AttributeChanges() = default;
    AttributeChanges(AttributeChanges&&) = default;
    AttributeChanges(AttributeChanges const&) = default;
    AttributeChanges& operator=(AttributeChanges const&) = default;
    AttributeChanges& operator=(AttributeChanges&&) = default;

    AttributeChanges(const std::string_view& view, size_t begin, size_t end, size_t size);
    char name[20];

    size_t attribute_size = 0;
    size_t change_range_begin = 0;
    size_t change_range_end = 0;

    static HighFive::CompoundType datatype();
};

inline AttributeChanges::AttributeChanges(
    const std::string_view& view,
    size_t begin,
    size_t end,
    size_t size)
    : attribute_size(size)
    , change_range_begin(begin)
    , change_range_end(end)
{
    strncpy(
        name,
        view.data(),
        sizeof(name) / sizeof(char)); // yes sizeof(char)==1, maybe chartype changes someday?
}
}
