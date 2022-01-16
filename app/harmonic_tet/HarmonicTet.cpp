#include "HarmonicTet.hpp"

#include <igl/predicates/predicates.h>
#include <wmtk/utils/TetraQualityUtils.hpp>
#include <wmtk/utils/TupleUtils.hpp>
#include <wmtk/utils/io.hpp>

#include <queue>
#include "wmtk/utils/EnergyHarmonicTet.hpp"

namespace harmonic_tet {

double HarmonicTet::get_quality(const Tuple& loc)
{
    Eigen::MatrixXd ps(4, 3);
    auto tups = oriented_tet_vertices(loc);
    for (auto j = 0; j < 4; j++) {
        ps.row(j) = m_vertex_attribute[tups[j].vid(*this)];
    }
    return wmtk::harmonic_energy(ps);
}


bool HarmonicTet::is_inverted(const Tuple& loc)
{
    std::array<Eigen::Vector3d, 4> ps;
    auto tups = oriented_tet_vertices(loc);
    for (auto j = 0; j < 4; j++) {
        ps[j] = m_vertex_attribute[tups[j].vid(*this)];
    }

    igl::predicates::exactinit();
    auto res = igl::predicates::orient3d(ps[0], ps[1], ps[2], ps[3]);
    if (res == igl::predicates::Orientation::NEGATIVE) return false; // extremely annoying.
    return true;
}

void HarmonicTet::swap_all_edges()
{
    auto queue = std::queue<std::tuple<double, wmtk::TetMesh::Tuple>>();

    for (auto& loc : get_edges()) {
        double length = -1.;
        queue.emplace(length, loc);
    }

    auto cnt_suc = 0;
    auto& m = *this;
    while (!queue.empty()) {
        auto [weight, loc] = queue.front();
        queue.pop();

        if (!loc.is_valid(*this)) continue;
        wmtk::TetMesh::Tuple newt;
        if (!swap_edge(loc, newt)) {
            continue;
        }
        auto new_edges = std::vector<wmtk::TetMesh::Tuple>();
        assert(newt.switch_tetrahedron(m));
        for (auto ti : {newt.tid(m), newt.switch_tetrahedron(m)->tid(m)}) {
            for (auto j = 0; j < 6; j++) new_edges.push_back(tuple_from_edge(ti, j));
        }
        wmtk::unique_edge_tuples(m, new_edges);
        for (auto& e : new_edges) {
            queue.emplace(-1., e);
        }
        cnt_suc++;
    }
}

bool HarmonicTet::swap_edge_before(const Tuple& t)
{
    if (!TetMesh::swap_edge_before(t)) return false;

    auto incident_tets = get_incident_tets_for_edge(t);
    auto total_energy = 0.;
    for (auto& l : incident_tets) {
        total_energy += (get_quality(l));
    }
    edgeswap_cache.total_energy = total_energy;
    return true;
}
bool HarmonicTet::swap_edge_after(const Tuple& t)
{
    if (!TetMesh::swap_edge_after(t)) return false;

    // after swap, t points to a face with 2 neighboring tets.
    auto oppo_tet = t.switch_tetrahedron(*this);
    assert(oppo_tet.has_value() && "Should not swap boundary.");
    auto total_energy = get_quality(t) + get_quality(*oppo_tet);
    wmtk::logger().debug("energy {} {}", edgeswap_cache.total_energy, total_energy);
    if (is_inverted(t) || is_inverted(*oppo_tet)) {
        wmtk::logger().debug("invert w/ energy {} {}", edgeswap_cache.total_energy, total_energy);
        return false;
    }
    if (total_energy > edgeswap_cache.total_energy) return false;
    return true;
}

void HarmonicTet::swap_all_faces()
{
    auto queue = std::queue<std::tuple<bool, wmtk::TetMesh::Tuple>>();

    for (auto& loc : get_faces()) {
        double length = -1.;
        queue.emplace(length, loc);
    }
    auto& m = *this;
    auto cnt_suc = 0;
    while (!queue.empty()) {
        auto [weight, loc] = queue.front();
        queue.pop();

        if (!loc.is_valid(m)) continue;
        wmtk::TetMesh::Tuple newt;
        if (!swap_face(loc, newt)) {
            continue;
        }
        auto new_tets = std::vector<size_t>(1, newt.tid(m));
        for (auto k = 0; k < 2; k++) {
            newt = newt.switch_face(m);
            newt = newt.switch_tetrahedron(m).value();
            new_tets.push_back(newt.tid(m));
        }

        auto new_faces = std::vector<wmtk::TetMesh::Tuple>();
        for (auto ti : new_tets) {
            for (auto j = 0; j < 4; j++) new_faces.push_back(tuple_from_face(ti, j));
        }
        wmtk::unique_face_tuples(m, new_faces);
        for (auto& e : new_faces) {
            queue.emplace(-1., e);
        }
        cnt_suc++;
    }
};
bool HarmonicTet::swap_face_before(const Tuple& t)
{
    if (!TetMesh::swap_face_before(t)) return false;

    auto oppo_tet = t.switch_tetrahedron(*this);
    assert(oppo_tet.has_value() && "Should not swap boundary.");
    faceswap_cache.total_energy = (get_quality(t) + get_quality(*oppo_tet));
    return true;
}

bool HarmonicTet::swap_face_after(const Tuple& t)
{
    if (!TetMesh::swap_face_after(t)) return false;

    auto incident_tets = get_incident_tets_for_edge(t);
    for (auto& l : incident_tets) {
        if (is_inverted(l)) {
            return false;
        }
    }
    auto total_energy = 0.;
    for (auto& l : incident_tets) {
        total_energy += get_quality(l);
    }
    wmtk::logger().trace("quality {} from {}", total_energy, faceswap_cache.total_energy);

    if (total_energy > faceswap_cache.total_energy) return false;
    return true;
}

void HarmonicTet::output_mesh(std::string file) const
{
    // warning: duplicate code.
    wmtk::MshData msh;

    const auto& vtx = get_vertices();
    msh.add_tet_vertices(vtx.size(), [&](size_t k) {
        auto i = vtx[k].vid(*this);
        return m_vertex_attribute[i];
    });

    const auto& tets = get_tets();
    msh.add_tets(tets.size(), [&](size_t k) {
        auto i = tets[k].tid(*this);
        auto vs = oriented_tet_vertices(tets[k]);
        std::array<size_t, 4> data;
        for (int j = 0; j < 4; j++) {
            data[j] = vs[j].vid(*this);
            assert(data[j] < vtx.size());
        }
        return data;
    });

    msh.add_tet_vertex_attribute<1>("tv index", [&](size_t i) { return i; });
    msh.add_tet_attribute<1>("t index", [&](size_t i) { return i; });

    msh.save(file, true);
}


void harmonic_tet::HarmonicTet::smooth_all_vertices()
{
    auto tuples = get_vertices();
    wmtk::logger().debug("tuples");
    auto cnt_suc = 0;
    for (auto& t : tuples) {
        if (smooth_vertex(t)) cnt_suc++;
    }
    wmtk::logger().debug("Smoothing Success Count {}", cnt_suc);
}

bool harmonic_tet::HarmonicTet::smooth_before(const Tuple& t)
{
    return true;
}

bool harmonic_tet::HarmonicTet::smooth_after(const Tuple& t)
{
    wmtk::logger().trace("Gradient Descent iteration for vertex smoothing.");
    auto vid = t.vid(*this);

    auto locs = get_one_ring_tets_for_vertex(t);
    assert(locs.size() > 0);
    std::vector<std::array<double, 12>> assembles(locs.size());
    auto loc_id = 0;

    for (auto& loc : locs) {
        auto& T = assembles[loc_id];
        auto t_id = loc.tid(*this);

        assert(!is_inverted(loc));
        auto local_tuples = oriented_tet_vertices(loc);
        std::array<size_t, 4> local_verts;
        for (auto i = 0; i < 4; i++) {
            local_verts[i] = local_tuples[i].vid(*this);
        }

        local_verts = wmtk::orient_preserve_tet_reorder(local_verts, vid);

        for (auto i = 0; i < 4; i++) {
            for (auto j = 0; j < 3; j++) {
                T[i * 3 + j] = m_vertex_attribute[local_verts[i]][j];
            }
        }
        loc_id++;
    }

    auto old_pos = m_vertex_attribute[vid];
    m_vertex_attribute[vid] = wmtk::gradient_descent_from_stack(
        assembles,
        wmtk::harmonic_tet_energy,
        wmtk::harmonic_tet_jacobian);
    wmtk::logger().trace(
        "old pos {} -> new pos {}",
        old_pos.transpose(),
        m_vertex_attribute[vid].transpose());
    // note: duplicate code snippets.
    for (auto& loc : locs) {
        if (is_inverted(loc)) {
            m_vertex_attribute[vid] = old_pos;
            return false;
        }
    }

    for (auto& loc : locs) {
        auto t_id = loc.tid(*this);
    }
    return true;
}

} // namespace harmonic_tet