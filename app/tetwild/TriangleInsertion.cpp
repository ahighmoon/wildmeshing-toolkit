//
// Created by Yixin Hu on 1/3/22.
//

#include <wmtk/utils/Delaunay.hpp>
#include "TetWild.h"
#include "wmtk/TetMesh.h"
#include "wmtk/auto_table.hpp"
#include "wmtk/utils/GeoUtils.h"
#include "wmtk/utils/Logger.hpp"

#include <igl/remove_duplicate_vertices.h>

#include <fstream>

using std::cout;
using std::endl;

void pausee() {
    std::cout << "pausing..." << std::endl;
    char c;
    std::cin >> c;
    if (c == '0') exit(0);
}

bool tetwild::TetWild::InputSurface::remove_duplicates()
{
    Eigen::MatrixXd V_tmp(vertices.size(), 3), V_in;
    Eigen::MatrixXi F_tmp(faces.size(), 3), F_in;
    for (int i = 0; i < vertices.size(); i++) V_tmp.row(i) = vertices[i];
    for (int i = 0; i < faces.size(); i++)
        F_tmp.row(i) << faces[i][0], faces[i][1], faces[i][2]; // note: using int here

    //
    Eigen::VectorXi IV, _;
    igl::remove_duplicate_vertices(V_tmp, F_tmp, SCALAR_ZERO * params.diag_l, V_in, IV, _, F_in);
    //
    for (int i = 0; i < F_in.rows(); i++) {
        int j_min = 0;
        for (int j = 1; j < 3; j++) {
            if (F_in(i, j) < F_in(i, j_min)) j_min = j;
        }
        if (j_min == 0) continue;
        int v0_id = F_in(i, j_min);
        int v1_id = F_in(i, (j_min + 1) % 3);
        int v2_id = F_in(i, (j_min + 2) % 3);
        F_in.row(i) << v0_id, v1_id, v2_id;
    }
    F_tmp.resize(0, 0);
    Eigen::VectorXi IF;
    igl::unique_rows(F_in, F_tmp, IF, _);
    F_in = F_tmp;
    //    std::vector<int> old_input_tags = input_tags;
    //    input_tags.resize(IF.rows());
    //    for (int i = 0; i < IF.rows(); i++) {
    //        input_tags[i] = old_input_tags[IF(i)];
    //    }
    //
    if (V_in.rows() == 0 || F_in.rows() == 0) return false;

    wmtk::logger().info("remove duplicates: ");
    wmtk::logger().info("#v: {} -> {}", vertices.size(), V_in.rows());
    wmtk::logger().info("#f: {} -> {}", faces.size(), F_in.rows());

    std::vector<Vector3d> out_vertices;
    std::vector<std::array<size_t, 3>> out_faces;
    out_vertices.resize(V_in.rows());
    out_faces.reserve(F_in.rows());
    //    old_input_tags = input_tags;
    //    input_tags.clear();
    for (int i = 0; i < V_in.rows(); i++) out_vertices[i] = V_in.row(i);

    for (int i = 0; i < F_in.rows(); i++) {
        if (F_in(i, 0) == F_in(i, 1) || F_in(i, 0) == F_in(i, 2) || F_in(i, 2) == F_in(i, 1))
            continue;
        if (i > 0 && (F_in(i, 0) == F_in(i - 1, 0) && F_in(i, 1) == F_in(i - 1, 2) &&
                      F_in(i, 2) == F_in(i - 1, 1)))
            continue;
        // check area
        Vector3d u = V_in.row(F_in(i, 1)) - V_in.row(F_in(i, 0));
        Vector3d v = V_in.row(F_in(i, 2)) - V_in.row(F_in(i, 0));
        Vector3d area = u.cross(v);
        if (area.norm() / 2 <= SCALAR_ZERO * params.diag_l) continue;
        out_faces.push_back({{(size_t)F_in(i, 0), (size_t)F_in(i, 1), (size_t)F_in(i, 2)}});
        //        input_tags.push_back(old_input_tags[i]);
    }

    vertices = out_vertices;
    faces = out_faces;

    return true;
}

void tetwild::TetWild::construct_background_mesh(const InputSurface& input_surface)
{
    const auto& vertices = input_surface.vertices;
    const auto& faces = input_surface.faces;

    ///points for delaunay
    std::vector<wmtk::Point3D> points(vertices.size());
    for (int i = 0; i < vertices.size(); i++) {
        for (int j = 0; j < 3; j++) points[i][j] = vertices[i][j];
    }
    ///box
    double delta = m_params.diag_l / 10.0;
    Vector3d box_min(m_params.min[0] - delta, m_params.min[1] - delta, m_params.min[2] - delta);
    Vector3d box_max(m_params.max[0] + delta, m_params.max[1] + delta, m_params.max[2] + delta);
    double Nx = std::max(2, int((box_max[0] - box_min[0]) / delta));
    double Ny = std::max(2, int((box_max[1] - box_min[1]) / delta));
    double Nz = std::max(2, int((box_max[2] - box_min[2]) / delta));
    for (int i = 0; i <= Nx; i++) {
        for (int j = 0; j <= Ny; j++) {
            for (int k = 0; k <= Nz; k++) {
                Vector3d p(
                    box_min[0] * (1 - i / Nx) + box_max[0] * i / Nx,
                    box_min[1] * (1 - j / Ny) + box_max[1] * j / Ny,
                    box_min[2] * (1 - k / Nz) + box_max[2] * k / Nz);
                if (!m_envelope.is_outside(p)) continue;
                points.push_back({{p[0], p[1], p[2]}});
            }
        }
    }

    ///delaunay
    auto tets = wmtk::delaunay3D_conn(points);
    wmtk::logger().info("tets.size() {}", tets.size());

    // conn
    init(points.size(), tets);
    // attr
    m_vertex_attribute.resize(points.size());
    m_tet_attribute.resize(tets.size());
    m_face_attribute.resize(tets.size() * 4);
    m_edge_attribute.resize(tets.size() * 6);
    for (int i = 0; i < m_vertex_attribute.size(); i++) {
        m_vertex_attribute[i].m_pos = Vector3(points[i][0], points[i][1], points[i][2]);
        m_vertex_attribute[i].m_posf = Vector3d(points[i][0], points[i][1], points[i][2]);
    }
    // todo: track bbox

    output_mesh("delaunay.msh");
}

void tetwild::TetWild::match_insertion_faces(
    const InputSurface& input_surface,
    std::vector<bool>& is_matched)
{
    const auto& vertices = input_surface.vertices;
    const auto& faces = input_surface.faces;
    is_matched.resize(faces.size(), false);

    std::map<std::array<size_t, 3>, size_t> map_surface;
    for (size_t i = 0; i < faces.size(); i++) {
        auto f = faces[i];
        std::sort(f.begin(), f.end());
        map_surface[f] = i;
    }
    for (size_t i = 0; i < m_tet_attribute.size(); i++) {
        Tuple t = tuple_from_tet(i);
        auto vs = oriented_tet_vertices(t);
        for (int j = 0; j < 4; j++) {
            std::array<size_t, 3> f = {
                {vs[(j + 1) % 4].vid(*this),
                 vs[(j + 2) % 4].vid(*this),
                 vs[(j + 3) % 4].vid(*this)}};
            std::sort(f.begin(), f.end());
            if (map_surface.count(f)) {
                int fid = map_surface[f];
                //                triangle_insertion_cache.surface_f_ids[i][j] = fid;
                triangle_insertion_cache.tet_face_tags[f].push_back(fid);
                is_matched[fid] = true;
            }
        }
    }
}

void tetwild::TetWild::triangle_insertion_before(const std::vector<Tuple>& faces)
{
    triangle_insertion_cache.old_face_vids.clear();//note: reset local vars

    for (auto& loc : faces) {
        auto vs = get_face_vertices(loc);
        std::array<size_t, 3> f = {{vs[0].vid(*this), vs[1].vid(*this), vs[2].vid(*this)}};
        std::sort(f.begin(), f.end());

        triangle_insertion_cache.old_face_vids.push_back(f);
    }
}

void tetwild::TetWild::triangle_insertion_after(
    const std::vector<Tuple>& old_faces,
    const std::vector<std::vector<Tuple>>& new_faces)
{
    auto& tet_face_tags = triangle_insertion_cache.tet_face_tags;

//    for(auto& info: triangle_insertion_cache.tet_face_tags) {
//        auto& vids = info.first;
//        auto& fids = info.second;
//        cout<<"vids "<<vids[0]<<" "<<vids[1]<<" "<<vids[2]<<endl;
//        cout<<"fids ";
//        for(int fid: fids)
//            cout<<fid<<" ";
//        cout<<endl;
//    }
//    cout<<"before"<<endl;
//    pausee();

    /// remove old_face_vids from tet_face_tags, and map tags to new faces
    assert(new_faces.size() == triangle_insertion_cache.old_face_vids.size() + 1);

    for (int i = 0; i < new_faces.size(); i++) {
        if (new_faces[i].empty()) continue;

        std::vector<int> tags;
        if (i < triangle_insertion_cache.old_face_vids.size()) {
            if(tet_face_tags.count(triangle_insertion_cache.old_face_vids[i]))
                tags = tet_face_tags[triangle_insertion_cache.old_face_vids[i]];
        } else
            tags = {triangle_insertion_cache.face_id};

        if(tags.empty())
            continue;

        for (auto& loc : new_faces[i]) {
            auto vs = get_face_vertices(loc);
            std::array<size_t, 3> f = {{vs[0].vid(*this), vs[1].vid(*this), vs[2].vid(*this)}};
            std::sort(f.begin(), f.end());
//            cout<<"f "<<f[0]<<" "<<f[1]<<" "<<f[2]<<" ("<<tags[0]<<endl;
            tet_face_tags[f] = tags;
        }

        if (i < triangle_insertion_cache.old_face_vids.size())
            tet_face_tags.erase(triangle_insertion_cache.old_face_vids[i]);
        // add new add erase old tag
    }

//    //fortest
//    const auto& vertices = triangle_insertion_cache.input_surface.vertices;
//    const auto& faces = triangle_insertion_cache.input_surface.faces;
//    for(auto& info: triangle_insertion_cache.tet_face_tags) {
//        auto& vids = info.first;
//        auto& fids = info.second;
//        cout<<vids[0]<<" "<<vids[1]<<" "<<vids[2]<<": ";
//        for(int fid: fids)
//            cout<<fid<<" ";
//        cout<<endl;
//
//        for (int fid : fids) {
//            std::array<Vector3, 3> tri = {
//                {to_rational(vertices[faces[fid][0]]),
//                 to_rational(vertices[faces[fid][1]]),
//                 to_rational(vertices[faces[fid][2]])}};
//            Vector3 tri_normal = (tri[1] - tri[0]).cross(tri[2] - tri[0]);
//            for (int j = 0; j < 3; j++) {
//                Vector3 v = m_vertex_attribute[vids[j]].m_pos - tri[0];
//                if (v.dot(tri_normal) != 0) {
//                    cout << "not coplanar!!!" << endl;
//                    cout << "tet v " << vids[j] << ", "
//                         << "face " << fid << " (" << faces[fid][0] << " " << faces[fid][1] << " "
//                         << faces[fid][2] << endl;
//                    pausee();
//                }
//            }
//        }
//    }
////    cout<<"after"<<endl;
////    pausee();
//    //fortest
}

void tetwild::TetWild::triangle_insertion(const InputSurface& _input_surface)
{
    const int EMPTY_INTERSECTION = 0;
    const int TRI_INTERSECTION = 1;
    const int PLN_INTERSECTION = 2;

    triangle_insertion_cache.input_surface = _input_surface; // todo: avoid copy
    const auto& input_surface = _input_surface;

    construct_background_mesh(input_surface);
    const auto& vertices = input_surface.vertices;
    const auto& faces = input_surface.faces;

    // fortest
//    auto pausee = []() {
//        std::cout << "pausing..." << std::endl;
//        char c;
//        std::cin >> c;
//        if (c == '0') exit(0);
//    };
    auto print = [](const Vector3& p) { wmtk::logger().info("{} {} {}", p[0], p[1], p[2]); };
    auto print2 = [](const Vector2& p) { wmtk::logger().info("{} {}", p[0], p[1]); };
    auto output_surface = [&](std::string file) {
        std::ofstream fout(file);
        std::vector<std::array<int, 3>> fs;
        int cnt = 0;

        for(auto& info: triangle_insertion_cache.tet_face_tags) {
            auto& vids = info.first;
            auto& fids = info.second;

            if (fids.empty()) continue;

            fout << "v " << m_vertex_attribute[vids[0]].m_posf.transpose() << std::endl;
            fout << "v " << m_vertex_attribute[vids[1]].m_posf.transpose() << std::endl;
            fout << "v " << m_vertex_attribute[vids[2]].m_posf.transpose() << std::endl;
            fs.push_back({{cnt * 3 + 1, cnt * 3 + 2, cnt * 3 + 3}});
            cnt++;
        }
        //        for (int i = 0; i < triangle_insertion_cache.surface_f_ids.size(); i++) {
        //            auto t = tuple_from_tet(i);
        //            auto vs = oriented_tet_vertices(t);
        //            std::array<size_t, 4> vids = {
        //                {vs[0].vid(*this), vs[1].vid(*this), vs[2].vid(*this), vs[3].vid(*this)}};
        //            for (int j = 0; j < 4; j++) {
        //                if (triangle_insertion_cache.surface_f_ids[i][j] >= 0) {
        //                    fout << "v " << m_vertex_attribute[vids[(j + 1) %
        //                    4]].m_posf.transpose()
        //                         << std::endl;
        //                    fout << "v " << m_vertex_attribute[vids[(j + 2) %
        //                    4]].m_posf.transpose()
        //                         << std::endl;
        //                    fout << "v " << m_vertex_attribute[vids[(j + 3) %
        //                    4]].m_posf.transpose()
        //                         << std::endl;
        //                    fs.push_back({{cnt * 3 + 1, cnt * 3 + 2, cnt * 3 + 3}});
        //                    cnt++;
        //                }
        //            }
        //        }
        for (auto& f : fs) fout << "f " << f[0] << " " << f[1] << " " << f[2] << std::endl;
        fout.close();
    };
    auto check_tracked_surface_coplanar = [&]() {
        for (int i = 0; i < triangle_insertion_cache.surface_f_ids.size(); i++) {
            Tuple tet = tuple_from_tet(i);
            auto tet_vertices = oriented_tet_vertices(tet);
            for (int j = 0; j < 4; j++) {
                int face_id = triangle_insertion_cache.surface_f_ids[i][j];
                if (face_id < 0) continue;

                Vector3 c = m_vertex_attribute[tet_vertices[(j + 1) % 4].vid(*this)].m_pos +
                            m_vertex_attribute[tet_vertices[(j + 2) % 4].vid(*this)].m_pos +
                            m_vertex_attribute[tet_vertices[(j + 3) % 4].vid(*this)].m_pos;
                c = c / 3;

                std::array<Vector3, 3> tri = {
                    {to_rational(vertices[faces[face_id][0]]),
                     to_rational(vertices[faces[face_id][1]]),
                     to_rational(vertices[faces[face_id][2]])}};

                bool is_coplanar = wmtk::segment_triangle_coplanar_3d({{c, c}}, tri);
                if (!is_coplanar) {
                    //                    cout << "!is_coplanar " << face_id << endl;
                    //                    cout << triangle_insertion_cache.face_id << endl;
                    print(c);
                    print(tri[0]);
                    print(tri[1]);
                    print(tri[2]);
                    pausee();
                }
            }
        }
    }; // todo
    // fortest


    //    triangle_insertion_cache.surface_f_ids.resize(m_tet_attribute.size(), {{-1, -1, -1, -1}});
    // match faces preserved in delaunay
    auto& is_matched = triangle_insertion_cache.is_matched;
    match_insertion_faces(input_surface, is_matched);
    wmtk::logger().info("is_matched: {}", std::count(is_matched.begin(), is_matched.end(), true));

    auto& is_visited = triangle_insertion_cache.is_visited;


    for (size_t face_id = 0; face_id < faces.size(); face_id++) {
        if (is_matched[face_id]) continue;

        triangle_insertion_cache.face_id = face_id;
        is_visited.assign(m_tet_attribute.size(), false); // reset

        std::array<Vector3, 3> tri = {
            {to_rational(vertices[faces[face_id][0]]),
             to_rational(vertices[faces[face_id][1]]),
             to_rational(vertices[faces[face_id][2]])}};
        //
        Vector3 tri_normal = (tri[1]-tri[0]).cross(tri[2]-tri[0]);
        //
        std::array<Vector2, 3> tri2;
        int squeeze_to_2d_dir = wmtk::project_triangle_to_2d(tri, tri2);

        wmtk::logger().info("face_id {}", face_id);
        //        cout<<faces[face_id][0]<<" "<<faces[face_id][1]<<" "<<faces[face_id][2]<<endl;
        //        print(tri[0]);
        //        print(tri[1]);
        //        print(tri[2]);
        //        print2(tri2[0]);
        //        print2(tri2[1]);
        //        print2(tri2[2]);


        std::vector<size_t> intersected_tids;
        std::vector<Tuple> intersected_tets;
        std::map<std::array<size_t, 2>, std::tuple<int, Vector3, size_t, int>> map_edge2point;
        // e = (e0, e1) ==> (intersection_status, intersection_point, edge's_tid, local_eid_in_tet)
        std::set<std::array<size_t, 2>> intersected_tet_edges;
        //
        std::queue<Tuple> tet_queue;
        //
        for (int j = 0; j < 3; j++) {
            Tuple loc = tuple_from_vertex(faces[face_id][j]);
            auto conn_tets = get_one_ring_tets_for_vertex(loc);
            for (const auto& t : conn_tets) {
                if(is_visited[t.tid(*this)])
                    continue;
                tet_queue.push(t);
                is_visited[t.tid(*this)] = true;
            }
        }
        //
        auto is_seg_cut_tri_2 = [](const std::array<Vector2, 2>& seg2,
                                   const std::array<Vector2, 3>& tri2){
            // overlap == seg has one endpoint inside tri OR seg intersect with tri edges
            bool is_inside = wmtk::is_point_inside_triangle(seg2[0], tri2) ||
                             wmtk::is_point_inside_triangle(seg2[1], tri2);
            if (is_inside) {
                return true;
            } else {
                for (int j = 0; j < 3; j++) {
                    apps::Rational _;
                    std::array<Vector2, 2> tri_seg2 = {{tri2[j], tri2[(j + 1) % 3]}};
                    bool is_intersected =
                        wmtk::open_segment_open_segment_intersection_2d(seg2, tri_seg2, _);
                    if (is_intersected) {
                        return true;
                    }
                }
            }
            return false;
        };
        while (!tet_queue.empty()) {
            auto add_onering_tets_of_tet = [&](std::array<Tuple, 4>& vs){
                for(auto& v: vs){
                    auto tets = get_one_ring_tets_for_vertex(v);
                    for(auto& t: tets){
                        if(is_visited[t.tid(*this)])
                            continue;
                        is_visited[t.tid(*this)] = true;
                        tet_queue.push(t);
                    }
                }
            };

            auto tet = tet_queue.front();
            tet_queue.pop();

            /// check vertices position
            int cnt_pos = 0;
            int cnt_neg = 0;
            std::map<size_t, int> vertex_sides;
            std::vector<int> coplanar_f_lvids;
            auto vs = oriented_tet_vertices(tet);
            std::array<size_t, 4> vertex_vids;
            for(int j=0;j<4;j++) {
                vertex_vids[j] = vs[j].vid(*this);
                Vector3 dir = m_vertex_attribute[vertex_vids[j]].m_pos - tri[0];
                auto side = dir.dot(tri_normal);
                if (side > 0) {
                    cnt_pos++;
                    vertex_sides[vertex_vids[j]] = 1;
                } else if (side < 0) {
                    cnt_neg++;
                    vertex_sides[vertex_vids[j]] = -1;
                } else {
//                    coplanar_f.push_back(vertex_vids[j]);
                    coplanar_f_lvids.push_back(j);
                    vertex_sides[vertex_vids[j]] = 0;
                }
            }
            //
            if (coplanar_f_lvids.size() == 1) {
                int lvid = coplanar_f_lvids[0];
                int vid = vertex_vids[lvid];
                Vector2 p =
                    wmtk::project_point_to_2d(m_vertex_attribute[vid].m_pos, squeeze_to_2d_dir);
                bool is_inside = wmtk::is_point_inside_triangle(p, tri2);
                //
                if(is_inside) {
                    auto conn_tets = get_one_ring_tets_for_vertex(vs[lvid]);
                    for (auto& t : conn_tets) {
                        if (is_visited[t.tid(*this)]) continue;
                        is_visited[t.tid(*this)] = true;
                        tet_queue.push(t);
                    }
                }
            } else if (coplanar_f_lvids.size() == 2) {
                std::array<Vector2, 2> seg2;
                seg2[0] = wmtk::project_point_to_2d(
                    m_vertex_attribute[vertex_vids[coplanar_f_lvids[0]]].m_pos,
                    squeeze_to_2d_dir);
                seg2[1] = wmtk::project_point_to_2d(
                    m_vertex_attribute[vertex_vids[coplanar_f_lvids[1]]].m_pos,
                    squeeze_to_2d_dir);
                if (is_seg_cut_tri_2(seg2, tri2)) {
                    for (int j = 0; j < 2; j++) {
                        auto conn_tets = get_one_ring_tets_for_vertex(vs[coplanar_f_lvids[j]]);
                        for (auto& t : conn_tets) {
                            if (is_visited[t.tid(*this)]) continue;
                            is_visited[t.tid(*this)] = true;
                            tet_queue.push(t);
                        }
                    }
                }
            } else if (coplanar_f_lvids.size() == 3){
                bool is_cut = false;
                for (int i = 0; i < 3; i++) {
                    std::array<Vector2, 2> seg2;
                    seg2[0] = wmtk::project_point_to_2d(
                        m_vertex_attribute[vertex_vids[coplanar_f_lvids[i]]].m_pos,
                        squeeze_to_2d_dir);
                    seg2[1] = wmtk::project_point_to_2d(
                        m_vertex_attribute[vertex_vids[coplanar_f_lvids[(i + 1) % 3]]].m_pos,
                        squeeze_to_2d_dir);
                    if (is_seg_cut_tri_2(seg2, tri2)) {
                        is_cut = true;
                        break;
                    }
                }
                if (is_cut) {
                    for (int j = 0; j < 3; j++) {
                        auto conn_tets = get_one_ring_tets_for_vertex(vs[coplanar_f_lvids[j]]);
                        for (auto& t : conn_tets) {
                            if (is_visited[t.tid(*this)]) continue;
                            is_visited[t.tid(*this)] = true;
                            tet_queue.push(t);
                        }
                    }
                }
            }
            //
            if(cnt_pos == 0 || cnt_neg == 0) {
                continue;
            }

            /// check edges
            std::array<Tuple, 6> edges = tet_edges(tet);
            //
            std::vector<std::array<size_t, 2>> edge_vids;
            for (auto& loc : edges) {
                size_t v1_id = loc.vid(*this);
                auto tmp = switch_vertex(loc);
                size_t v2_id = tmp.vid(*this);
                std::array<size_t, 2> e = {{v1_id, v2_id}};
                if (e[0] > e[1]) std::swap(e[0], e[1]);
                edge_vids.push_back(e);
            }

//            /// check if tet face overlaps with the surface_f, if so, update surface_f_ids
//            // tet face overlaps with tri == have exactly 3 edges of tet coplanar with tri
//            int cnt_coplanar = 0;
//            int cnt_intersected = 0;
//            for (auto& e : edge_vids) {
//                std::array<Vector3, 2> seg = {
//                    {m_vertex_attribute[e[0]].m_pos, m_vertex_attribute[e[1]].m_pos}};
//                bool is_coplanar = wmtk::segment_triangle_coplanar_3d(seg, tri);
//                if (is_coplanar) {
//                    coplanar_f.push_back(e[0]);
//                    coplanar_f.push_back(e[1]);
//                    cnt_coplanar++;
//
//                    if (cnt_intersected > 0) continue;
//
//                    std::array<Vector2, 2> seg2;
//                    seg2[0] = wmtk::project_point_to_2d(seg[0], squeeze_to_2d_dir);
//                    seg2[1] = wmtk::project_point_to_2d(seg[1], squeeze_to_2d_dir);
//                    // overlap == seg has one endpoint inside tri OR seg intersect with tri edges
//                    bool is_inside = wmtk::is_point_inside_triangle(seg2[0], tri2) ||
//                                     wmtk::is_point_inside_triangle(seg2[1], tri2);
//                    if (is_inside) {
//                        cnt_intersected++;
//                    } else {
//                        for (int j = 0; j < 3; j++) {
//                            apps::Rational _;
//                            std::array<Vector2, 2> tri_seg2 = {{tri2[j], tri2[(j + 1) % 3]}};
//                            bool is_intersected =
//                                wmtk::open_segment_open_segment_intersection_2d(seg2, tri_seg2, _);
//                            if (is_intersected) {
//                                cnt_intersected++;
//                                break;
//                            }
//                        }
//                    }
//                }
//                if (cnt_coplanar == 3) break;
//            }
//            if (cnt_coplanar == 3 && cnt_intersected > 0) {
//                wmtk::vector_unique(coplanar_f);
//                assert(coplanar_f.size() == 3);
//                triangle_insertion_cache
//                    .tet_face_tags[{{coplanar_f[0], coplanar_f[1], coplanar_f[2]}}]
//                    .push_back(face_id);
//                cout<<"COPLANAR!!!"<<endl;
//                //todo: add tet into queue??
//                continue;
//            }


            /// check if the tet edges intersects with the triangle
            bool need_subdivision = false;
            for (int l_eid = 0; l_eid < edges.size(); l_eid++) {
                const std::array<size_t, 2>& e = edge_vids[l_eid];
                if(vertex_sides[e[0]] * vertex_sides[e[1]] >= 0)
                    continue;

                if (map_edge2point.count(e)) {
                    if (std::get<0>(map_edge2point[e]) == TRI_INTERSECTION) need_subdivision = true;
                    continue;
                }

                std::array<Vector3, 2> seg = {
                    {m_vertex_attribute[e[0]].m_pos, m_vertex_attribute[e[1]].m_pos}};
                Vector3 p(0, 0, 0);
                int intersection_status = EMPTY_INTERSECTION;

                //                bool is_coplanar = wmtk::segment_triangle_coplanar_3d(seg, tri);
                //                if (is_coplanar) {
                ////                    cout << "is_coplanar" << endl;
                //                    // todo: different way of marking surface, move to above ^
                //                    std::array<Vector2, 2> seg2;
                //                    seg2[0] = wmtk::project_point_to_2d(seg[0],
                //                    squeeze_to_2d_dir); seg2[1] =
                //                    wmtk::project_point_to_2d(seg[1], squeeze_to_2d_dir); for (int
                //                    j = 0; j < 3; j++) {
                //                        apps::Rational t1;
                //                        std::array<Vector2, 2> tri_seg2 = {{tri2[j], tri2[(j + 1)
                //                        % 3]}}; bool is_intersected =
                //                            wmtk::open_segment_open_segment_intersection_2d(seg2,
                //                            tri_seg2, t1);
                //                        if (is_intersected) {
                //                            intersection_status = TRI_INTERSECTION;
                //                            p = (1 - t1) * seg[0] + t1 * seg[1];
                //                            break;
                //                        }
                //                    }
                //                } else {
                //                    is_intersected =
                //                    wmtk::open_segment_triangle_intersection_3d(seg, tri, p);
                bool is_inside_tri = false;
                bool is_intersected_plane =
                    wmtk::open_segment_plane_intersection_3d(seg, tri, p, is_inside_tri);
                if (is_intersected_plane && is_inside_tri) {
                    intersection_status = TRI_INTERSECTION;
                } else if (is_intersected_plane) {
                    intersection_status = PLN_INTERSECTION;
                }
                //                }

                map_edge2point[e] = std::make_tuple(intersection_status, p, tet.tid(*this), l_eid);
                if (intersection_status == EMPTY_INTERSECTION) {
                    continue;
                } else if (intersection_status == TRI_INTERSECTION) {
                    need_subdivision = true;
                }

                // add new tets
                if(need_subdivision) {
                    auto incident_tets = get_incident_tets_for_edge(edges[l_eid]);
                    for (auto& t : incident_tets) {
                        int tid = t.tid(*this);
                        if (is_visited[tid]) continue;
                        tet_queue.push(t);
                        is_visited[tid] = true;
                    }
                }
            }

            /// check if the tet (open) face intersects with the triangle
            if (!need_subdivision) {
                std::array<Tuple, 4> fs;
                fs[0] = tet;
                fs[1] = switch_face(tet);
                auto tet1 = switch_edge(switch_vertex(tet));
                fs[2] = switch_face(tet1);
                auto tet2 = switch_edge(switch_vertex(tet1));
                fs[3] = switch_face(tet2);

                for (int j = 0; j < 4; j++) { // for each tet face
                    auto vs = get_face_vertices(tet);

                    {
                        int cnt_pos1 = 0;
                        int cnt_neg1 = 0;
                        for (int k = 0; k < 3; k++) {
                            if (vertex_sides[vs[k].vid(*this)] > 0)
                                cnt_pos1++;
                            else if (vertex_sides[vs[k].vid(*this)] < 0)
                                cnt_neg1++;
                        }
                        if (cnt_pos1 == 0 || cnt_neg1 == 0) continue;
                    }

                    std::array<Vector3, 3> tet_tri = {{
                        m_vertex_attribute[vs[0].vid(*this)].m_pos,
                        m_vertex_attribute[vs[1].vid(*this)].m_pos,
                        m_vertex_attribute[vs[2].vid(*this)].m_pos,
                    }};

                    bool is_intersected = false;
                    for (int k = 0; k < 3; k++) { // check intersection
                        std::array<Vector3, 2> input_seg = {{tri[k], tri[(k + 1) % 3]}};
                        Vector3 p;
                        is_intersected =
                            wmtk::open_segment_triangle_intersection_3d(input_seg, tet_tri, p);
                        if (is_intersected) {
                            need_subdivision = true; // todo: can be recorded
                            break;
                        }
                    }

                    if (is_intersected) {
                        auto res = switch_tetrahedron(tet);
                        if (res.has_value()) {
                            auto n_tet = res.value();
                            int tid = n_tet.tid(*this);
                            if (is_visited[tid]) continue;
                            tet_queue.push(n_tet);
                            is_visited[tid] = true;
                        }
                    }
                }
            }


            /// record the tets
            if (need_subdivision) {
                //                intersected_tids.push_back(tet.tid(*this));
                intersected_tets.push_back(tet);
                //
                for (auto& e : edge_vids) {
                    intersected_tet_edges.insert(e);
                }
            }
        }
        //        wmtk::vector_unique(intersected_tids);//note: not needed
        wmtk::logger().info("intersected_tets.size {}", intersected_tets.size());

        ///use plane to cut intersected_tids and get surrounding_tids
        //        std::vector<size_t> surrounding_tids;
        //        for (size_t tid : intersected_tids) {
        //            auto tet = tuple_from_tet(tid);
        //            auto edges = tet_edges(tet);
        //            for (auto& loc : edges) {
        //                size_t v1_id = loc.vid(*this);
        //                auto tmp = switch_vertex(loc);
        //                size_t v2_id = tmp.vid(*this);
        //                std::array<size_t, 2> e = {{v1_id, v2_id}};
        //                if (e[0] > e[1]) std::swap(e[0], e[1]);
        //
        //                intersected_tet_edges.insert(e);
        //
        //                if (map_edge2point.count(e) &&
        //                    map_edge2point[e].first == PLN_INTERSECTION) { // todo: wrong
        //                    auto incident_tets = get_incident_tets_for_edge(loc);
        //                    for (auto& t : incident_tets) {
        //                        int tid = t.tid(*this);
        //                        surrounding_tids.push_back(tid);
        //                    }
        //                }
        //            }
        //        }
        //        //
        //        wmtk::vector_unique(surrounding_tids);
        //        std::vector<size_t> diff_tids;
        //        std::set_difference(
        //            surrounding_tids.begin(),
        //            surrounding_tids.end(),
        //            intersected_tids.begin(),
        //            intersected_tids.end(),
        //            std::back_inserter(diff_tids));
        //        //
        //        std::vector<bool> mark_surface(intersected_tids.size(), true);
        //        for (int i = 0; i < diff_tids.size(); i++) mark_surface.push_back(false);
        //        //
        //        intersected_tids.insert(intersected_tids.end(), diff_tids.begin(),
        //        diff_tids.end());

        // erase edge without intersections OR edge with intersection but not belong to intersected
        // tets
        for (auto it = map_edge2point.begin(), ite = map_edge2point.end(); it != ite;) {
            if (std::get<0>(it->second) == EMPTY_INTERSECTION ||
                !intersected_tet_edges.count(it->first))
                it = map_edge2point.erase(it);
            else
                ++it;
        }

        ///push back new vertices
        std::vector<Tuple> intersected_edges;
        std::map<std::array<size_t, 2>, size_t> map_edge2vid;
        for (auto& info : map_edge2point) {
            auto& [_, p, tid, l_eid] = info.second;
            VertexAttributes v;
            v.m_pos = p;
            v.m_posf = to_double(v.m_pos);
            m_vertex_attribute.push_back(v);
            //            (info.second).first = m_vertex_attribute.size() - 1;//todo: check if needed

            map_edge2vid[info.first] = m_vertex_attribute.size() - 1;

            ///
            intersected_edges.push_back(tuple_from_edge(tid, l_eid));
        }
        wmtk::logger().info("before v.size {}", vert_capacity());
        wmtk::logger().info("after v.size {}", m_vertex_attribute.size());
        wmtk::logger().info("map_edge2vid.size {}", map_edge2vid.size());

        ///inert a triangle
        single_triangle_insertion(intersected_tets, intersected_edges);


        ///resize attri lists
        m_tet_attribute.resize(tet_capacity()); // todo: do we need it?

        check_mesh_connectivity_validity();
        wmtk::logger().info("inserted #t {}", tet_capacity());
        wmtk::logger().info("tet_face_tags.size {}", triangle_insertion_cache.tet_face_tags.size());

        //        pausee();

        //        //fortest
        //        check_tracked_surface_coplanar();
        //        //fortest
    }

    // fortest
    output_surface("surface0.obj");
    // fortest

    /// update m_is_on_surface for vertices, remove leaked surface marks
    m_edge_attribute.resize(m_tet_attribute.size() * 6);
    m_face_attribute.resize(m_tet_attribute.size() * 4);
    for(auto& info: triangle_insertion_cache.tet_face_tags) {
        auto& vids = info.first;
        auto& fids = info.second;

        Vector3 c = m_vertex_attribute[vids[0]].m_pos + m_vertex_attribute[vids[1]].m_pos +
                    m_vertex_attribute[vids[2]].m_pos;
        c = c / 3;

        wmtk::vector_unique(fids);

        int inside_fid = -1;
        for (int fid : fids) {
            std::array<Vector3, 3> tri = {
                {to_rational(vertices[faces[fid][0]]),
                 to_rational(vertices[faces[fid][1]]),
                 to_rational(vertices[faces[fid][2]])}};
            //
            std::array<Vector2, 3> tri2;
            int squeeze_to_2d_dir = wmtk::project_triangle_to_2d(tri, tri2);
            auto c2 = wmtk::project_point_to_2d(c, squeeze_to_2d_dir);
            //
            if (wmtk::is_point_inside_triangle(c2, tri2)) {//todo: should exclude the points on the edges of tri2 -- NO
                // todo: update m_face_attribute

                inside_fid = fid;
                break;
            }
        }

        //fortest
        if (inside_fid >= 0) {
            fids = {inside_fid};
        } else {
            fids = {};
        }
        //fortest
    }

    //    for (int i = 0; i < triangle_insertion_cache.surface_f_ids.size(); i++) {
    //        Tuple tet = tuple_from_tet(i);
    //        auto tet_vertices = oriented_tet_vertices(tet);
    //        for (int j = 0; j < 4; j++) {
    //            int face_id = triangle_insertion_cache.surface_f_ids[i][j];
    //            if (face_id < 0) continue;
    //
    //            Vector3 c = m_vertex_attribute[tet_vertices[(j + 1) % 4].vid(*this)].m_pos +
    //                        m_vertex_attribute[tet_vertices[(j + 2) % 4].vid(*this)].m_pos +
    //                        m_vertex_attribute[tet_vertices[(j + 3) % 4].vid(*this)].m_pos;
    //            c = c / 3;
    //
    //            std::array<Vector3, 3> tri = {
    //                {to_rational(vertices[faces[face_id][0]]),
    //                 to_rational(vertices[faces[face_id][1]]),
    //                 to_rational(vertices[faces[face_id][2]])}};
    //            //            //fortest
    //            bool is_coplanar = wmtk::segment_triangle_coplanar_3d({{c, c}}, tri);
    //            if (!is_coplanar) {
    //                triangle_insertion_cache.surface_f_ids[i][j] = -1; // fortest
    //
    //                //                cout<<"!is_coplanar"<<endl;
    //                //                print(c);
    //                //                print(tri[0]);
    //                //                print(tri[1]);
    //                //                print(tri[2]);
    //                //                pausee();
    //            }
    //            // fortest
    //
    //            std::array<Vector2, 3> tri2;
    //            int squeeze_to_2d_dir = wmtk::project_triangle_to_2d(tri, tri2);
    //            auto c2 = wmtk::project_point_to_2d(c, squeeze_to_2d_dir);
    //
    //            if (wmtk::is_point_inside_triangle(c2, tri2)) {
    //                Tuple face = tuple_from_face(i, j);
    //                int global_fid = face.fid(*this);
    //                m_face_attribute[global_fid].m_surface_tags = face_id;
    //            } else {
    //                triangle_insertion_cache.surface_f_ids[i][j] = -1; // fortest
    //            }
    //        }
    //    }

    /// todo: track bbox

    check_mesh_connectivity_validity();
    output_mesh("triangle_insertion.msh");

    // fortest
    output_surface("surface.obj");
    // fortest

} // note: skip preserve open boundaries

// void tetwild::TetWild::insertion_update_surface_tag(
//     size_t t_id,
//     size_t new_t_id,
//     int config_id,
//     int diag_config_id,
//     int index,
//     bool mark_surface) {
//    //    const auto &config = wmtk::CutTable::get_tet_conf(config_id, diag_config_id);
//    const auto &new_is_surface_fs = wmtk::CutTable::get_surface_conf(config_id, diag_config_id);
//    const auto &old_local_f_ids = wmtk::CutTable::get_face_id_conf(config_id, diag_config_id);
//
//    // track surface ==> t_id, new_t_id, is_surface_fs for new_t_id, local_f_ids of t_id mapped to
//    // new_t_id, face_id (cached)
//    if (t_id != new_t_id) {
//        triangle_insertion_cache.surface_f_ids.push_back({{-1, -1, -1, -1}});
//    }
//    //
//    auto old_surface_f_ids = triangle_insertion_cache.surface_f_ids[t_id];
//    //
//    for (int j = 0; j < 4; j++) {
//        if (old_local_f_ids[index][j] >= 0 && old_surface_f_ids[old_local_f_ids[index][j]] >= 0) {
//            triangle_insertion_cache.surface_f_ids[new_t_id][j] =
//                old_surface_f_ids[old_local_f_ids[index][j]];
//        }
//
//        if (mark_surface && new_is_surface_fs[index][j])
//            triangle_insertion_cache.surface_f_ids[new_t_id][j] =
//            triangle_insertion_cache.face_id;
//        // note: new face_id has higher priority than old ones
//        // note: non-cut-through tet does not track surface!!!
//    }
//}

void tetwild::TetWild::add_tet_centroid(const std::array<size_t, 4>& vids)
{
    //    auto vs = oriented_tet_vertices(t);
    VertexAttributes v;
    v.m_pos = (m_vertex_attribute[vids[0]].m_pos + m_vertex_attribute[vids[1]].m_pos +
               m_vertex_attribute[vids[2]].m_pos + m_vertex_attribute[vids[3]].m_pos) /
              4;
    //        (m_vertex_attribute[vs[0].vid(*this)].m_pos +
    //        m_vertex_attribute[vs[1].vid(*this)].m_pos +
    //         m_vertex_attribute[vs[2].vid(*this)].m_pos + m_vertex_attribute[vs[3].vid(*this)].m_pos) /
    //        4;
    v.m_posf = to_double(v.m_pos);
    m_vertex_attribute.push_back(v);
}