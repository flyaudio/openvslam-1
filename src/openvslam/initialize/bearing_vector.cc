#include "openvslam/data/frame.h"
#include "openvslam/initialize/bearing_vector.h"
#include "openvslam/solve/essential_solver.h"

#include <spdlog/spdlog.h>

namespace openvslam {
namespace initialize {

bearing_vector::bearing_vector(const data::frame& ref_frm, const unsigned int max_num_iters)
        : base(ref_frm, max_num_iters) {
    spdlog::debug("CONSTRUCT: initialize::bearing_vector");
}

bearing_vector::~bearing_vector() {
    spdlog::debug("DESTRUCT: initialize::bearing_vector");
}

bool bearing_vector::initialize(const data::frame& cur_frm, const std::vector<int>& ref_matches_with_cur) {
    // カメラモデルをセット
    cur_camera_ = cur_frm.camera_;
    // 特徴点を保存
    cur_undist_keypts_ = cur_frm.undist_keypts_;
    cur_bearings_ = cur_frm.bearings_;
    // matching情報を整形
    ref_cur_matches_.clear();
    ref_cur_matches_.reserve(cur_frm.undist_keypts_.size());
    for (unsigned int ref_idx = 0; ref_idx < ref_matches_with_cur.size(); ++ref_idx) {
        const auto cur_idx = ref_matches_with_cur.at(ref_idx);
        if (0 <= cur_idx) {
            ref_cur_matches_.emplace_back(std::make_pair(ref_idx, cur_idx));
        }
    }

    // Eを計算
    auto essential_solver = solve::essential_solver(ref_bearings_, cur_bearings_, ref_cur_matches_);
    essential_solver.find_via_ransac(max_num_iters_);

    // 3次元点を作成
    if (essential_solver.solution_is_valid()) {
        const Mat33_t E_ref_to_cur = essential_solver.get_best_E_21();
        const auto is_inlier_match = essential_solver.get_inlier_matches();
        return reconstruct_with_E(E_ref_to_cur, is_inlier_match);
    }
    else {
        return false;
    }
}

bool bearing_vector::reconstruct_with_E(const Mat33_t& E_ref_to_cur, const std::vector<bool>& is_inlier_match) {
    // 4個の姿勢候補に対して，3次元点をtriangulationして有効な3次元点の数を数える

    // E行列を分解
    eigen_alloc_vector<Mat33_t> init_rots;
    eigen_alloc_vector<Vec3_t> init_transes;
    if (!solve::essential_solver::decompose(E_ref_to_cur, init_rots, init_transes)) {
        return false;
    }

    assert(init_rots.size() == 4);
    assert(init_transes.size() == 4);

    const auto pose_is_found = find_most_plausible_pose(init_rots, init_transes, is_inlier_match, false);
    if (!pose_is_found) {
        return false;
    }

    spdlog::info("initialization succeeded with E");
    return true;
}

} // namespace initialize
} // namespace openvslam
