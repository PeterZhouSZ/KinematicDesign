#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "LinkageSynthesis.h"
#include "Solution.h"
#include "BBox.h"

namespace kinematics {

	class LinkageSynthesisRRRP : public LinkageSynthesis {
	public:
		LinkageSynthesisRRRP() {}

	public:
		void calculateSolution(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, int num_samples, std::vector<Object25D>& fixed_body_pts, const Object25D& body_pts, std::vector<std::pair<double, double>>& sigmas, bool rotatable_crank, bool avoid_branch_defect, double min_link_length, std::vector<Solution>& solutions);
		bool optimizeCandidate(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const BBox& bbox, std::vector<glm::dvec2>& points);
		bool optimizeLink(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const BBox& bbox, glm::dvec2& A0, glm::dvec2& A1);
		bool optimizeLinkForThreePoses(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, glm::dvec2& A0, glm::dvec2& A1);
		bool optimizeLinkForTwoPoses(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, glm::dvec2& A0, glm::dvec2& A1);
		bool optimizeSlider(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const BBox& bbox, glm::dvec2& A0, glm::dvec2& A1);
		Solution findBestSolution(const std::vector<glm::dmat3x3>& poses, std::vector<Solution>& solutions, const std::vector<glm::dvec2>& linkage_region_pts, const cv::Mat& dist_map, const BBox& dist_map_bbox, const std::vector<glm::dvec2>& linkage_avoidance_pts, std::vector<Object25D>& fixed_body_pts, const Object25D& body_pts, bool rotatable_crank, bool avoid_branch_defect, double min_link_length, double position_error_weight, double orientation_error_weight, double linkage_location_weight, double smoothness_weight, double size_weight);
		double calculateCost(Solution& solution, const Object25D& body_pts, const cv::Mat& dist_map, const BBox& dist_map_bbox, double position_error_weight, double orientation_error_weight, double linkage_location_weight, double smoothness_weight, double size_weight);
		Kinematics constructKinematics(const std::vector<glm::dvec2>& points, const std::vector<std::vector<int>>& zorder, const Object25D& body_pts, bool connect_joints, std::vector<Object25D>& fixed_body_pts = std::vector<Object25D>());
		void updateBodies(Kinematics& kinematics, const Object25D& body_pts);

		bool checkHardConstraints(std::vector<glm::dvec2>& points, const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, std::vector<Object25D>& fixed_body_pts, const Object25D& body_pts, bool rotatable_crank, bool avoid_branch_defect, double min_link_length, std::vector<std::vector<int>>& zorder);
		int getType(const std::vector<glm::dvec2>& points);
		bool checkRotatableCrankDefect(const std::vector<glm::dvec2>& points);
		bool checkOrderDefect(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points);
		bool checkBranchDefect(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points);
		bool checkCircuitDefect(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points);
		bool checkCollision(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points, std::vector<Object25D> fixed_body_pts, const Object25D& body_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, glm::dvec2& slider_end_pos1, glm::dvec2& slider_end_pos2, int collision_check_type);
		Kinematics recordCollisionForConnectors(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points, std::vector<Object25D> fixed_body_pts, const Object25D& body_pts);
		double tortuosityOfTrajectory(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points, const Object25D& body_pts);
	};

}