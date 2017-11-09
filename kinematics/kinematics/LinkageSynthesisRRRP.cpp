﻿#include "LinkageSynthesisRRRP.h"
#include "KinematicUtils.h"
#include "Kinematics.h"
#include "PinJoint.h"
#include "SliderHinge.h"
#include "BoundingBox.h"
#include "LeastSquareSolver.h"
#include "ZOrder.h"
#include <opencv2/opencv.hpp>

namespace kinematics {
	
	/**
	* Calculate solutions of RRRP linkage given three poses.
	*
	* @param poses			three poses
	* @param solutions1	the output solutions for the driving crank, each of which contains a pair of the center point and the circle point
	* @param solutions2	the output solutions for the follower, each of which contains a pair of the fixed point and the slider point
	*/
	void LinkageSynthesisRRRP::calculateSolution(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, int num_samples, std::vector<Object25D>& fixed_body_pts, const Object25D& body_pts, std::vector<std::pair<double, double>>& sigmas, bool rotatable_crank, bool avoid_branch_defect, double min_link_length, std::vector<Solution>& solutions) {
		solutions.clear();

		srand(0);

		// calculate the center of the valid regions
		BBox bbox = boundingBox(linkage_region_pts);
		glm::dvec2 bbox_center = bbox.center();

		int cnt = 0;
		for (int scale = 1; scale <= 3 && cnt == 0; scale++) {
		//for (int scale = 1; scale <= 3 && cnt < num_samples; scale++) {
			// calculate the enlarged linkage region for the sampling region
			std::vector<glm::dvec2> enlarged_linkage_region_pts;
			for (int i = 0; i < linkage_region_pts.size(); i++) {
				enlarged_linkage_region_pts.push_back((linkage_region_pts[i] - bbox_center) * (double)scale + bbox_center);
			}

			// calculate the bounding boxe of the valid regions
			BBox enlarged_bbox = boundingBox(enlarged_linkage_region_pts);

			// calculate the distace transform of the linkage region
			cv::Mat distMap;
			createDistanceMapForLinkageRegion(linkage_region_pts, enlarged_bbox, distMap);

			for (int iter = 0; iter < num_samples * 100 && cnt < num_samples; iter++) {
				printf("\rsampling %d/%d", cnt, (scale - 1) * num_samples * 100 + iter + 1);

				// perturbe the poses a little
				double position_error = 0.0;
				double orientation_error = 0.0;
				std::vector<glm::dmat3x3> perturbed_poses = perturbPoses(poses, sigmas, position_error, orientation_error);

				// sample joints within the linkage region
				std::vector<glm::dvec2> points(5);
				for (int i = 0; i < points.size(); i++) {
					while (true) {
						points[i] = glm::dvec2(genRand(enlarged_bbox.minPt.x, enlarged_bbox.maxPt.x), genRand(enlarged_bbox.minPt.y, enlarged_bbox.maxPt.y));
						if (withinPolygon(enlarged_linkage_region_pts, points[i])) break;
					}
				}

				if (!optimizeCandidate(perturbed_poses, enlarged_linkage_region_pts, enlarged_bbox, points)) continue;

				// check hard constraints
				std::vector<std::vector<int>> zorder;
				if (!checkHardConstraints(points, perturbed_poses, enlarged_linkage_region_pts, linkage_avoidance_pts, fixed_body_pts, body_pts, rotatable_crank, avoid_branch_defect, min_link_length, zorder)) continue;
				
				// collision check again
				// beucase B2 (the other end of the bar) is added to the linkage.
				//if (checkCollision(perturbed_poses, { A0, B0, A1, B1, B2 }, fixed_body_pts, body_pts[0], slider_end_pos1, slider_end_pos2, 1)) continue;

				// calculate the distance of the joints from the user-specified linkage region
				double dist = 0.0;
				for (int i = 0; i < points.size(); i++) {
					dist += distMap.at<double>(points[i].y - enlarged_bbox.minPt.y, points[i].x - enlarged_bbox.minPt.x);
				}

				solutions.push_back(Solution(points, position_error, orientation_error, dist, perturbed_poses, enlarged_linkage_region_pts, enlarged_bbox, zorder));
				cnt++;
			}
		}
		printf("\n");
	}

	/**
	* Optimize the linkage parameter based on the rigidity constraints.
	* If it fails to opotimize, return false.
	*/
	bool LinkageSynthesisRRRP::optimizeCandidate(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const BBox& bbox_world, std::vector<glm::dvec2>& points) {
		if (poses.size() == 2) {
			if (!optimizeLinkForTwoPoses(poses, linkage_region_pts, points[0], points[2])) return false;
		}
		else if (poses.size() == 3) {
			if (!optimizeLinkForThreePoses(poses, linkage_region_pts, points[0], points[2])) return false;
		}
		else {
			if (!optimizeLink(poses, linkage_region_pts, bbox_world, points[0], points[2])) return false;
		}

		if (!optimizeSlider(poses, linkage_region_pts, bbox_world, points[1], points[3])) return false;

		return true;
	}

	bool LinkageSynthesisRRRP::optimizeLink(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const BBox& bbox_world, glm::dvec2& A0, glm::dvec2& A1) {
		// setup the initial parameters for optimization
		column_vector starting_point(4);
		column_vector lower_bound(4);
		column_vector upper_bound(4);
		starting_point(0, 0) = A0.x;
		starting_point(1, 0) = A0.y;
		starting_point(2, 0) = A1.x;
		starting_point(3, 0) = A1.y;
		lower_bound(0, 0) = bbox_world.minPt.x;
		lower_bound(1, 0) = bbox_world.minPt.y;
		lower_bound(2, 0) = bbox_world.minPt.x;
		lower_bound(3, 0) = bbox_world.minPt.y;
		upper_bound(0, 0) = bbox_world.maxPt.x;
		upper_bound(1, 0) = bbox_world.maxPt.y;
		upper_bound(2, 0) = bbox_world.maxPt.x;
		upper_bound(3, 0) = bbox_world.maxPt.y;

		double min_range = std::numeric_limits<double>::max();
		for (int i = 0; i < 4; i++) {
			min_range = std::min(min_range, upper_bound(i, 0) - lower_bound(i, 0));
		}

		try {
			find_min_bobyqa(SolverForLink(poses), starting_point, 14, lower_bound, upper_bound, min_range * 0.19, min_range * 0.0001, 1000);

			A0.x = starting_point(0, 0);
			A0.y = starting_point(1, 0);
			A1.x = starting_point(2, 0);
			A1.y = starting_point(3, 0);

			// if the joints are outside the valid region, discard it.
			if (!withinPolygon(linkage_region_pts, A0)) return false;
			if (!withinPolygon(linkage_region_pts, A1)) return false;
		}
		catch (std::exception& e) {
			return false;
		}

		return true;
	}

	bool LinkageSynthesisRRRP::optimizeLinkForThreePoses(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, glm::dvec2& A0, glm::dvec2& A1) {
		// sample a point within the valid region as the local coordinate of a circle point
		glm::dvec2 a = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(A1, 1));

		glm::dvec2 A2(poses[1] * glm::dvec3(a, 1));
		glm::dvec2 A3(poses[2] * glm::dvec3(a, 1));

		try {
			A0 = circleCenterFromThreePoints(A1, A2, A3);

			// if the center point is outside the valid region, discard it.
			if (!withinPolygon(linkage_region_pts, A0)) return false;

			// if the moving point is outside the valid region, discard it.
			//if (!withinPolygon(linkage_region_pts, A1)) return false;
		}
		catch (char* ex) {
			return false;
		}

		return true;
	}

	bool LinkageSynthesisRRRP::optimizeLinkForTwoPoses(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, glm::dvec2& A0, glm::dvec2& A1) {
		// calculate the local coordinate of A1
		glm::dvec2 a = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(A1, 1));

		glm::dvec2 A2(poses[1] * glm::dvec3(a, 1));

		glm::dvec2 M = (A1 + A2) * 0.5;
		glm::dvec2 v = A1 - A2;
		v /= glm::length(v);
		glm::dvec2 h(-v.y, v.x);

		A0 = M + h * glm::dot(A0 - M, h);

		// if the center point is outside the valid region, discard it.
		if (!withinPolygon(linkage_region_pts, A0)) return false;

		return true;
	}

	bool LinkageSynthesisRRRP::optimizeSlider(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const BBox& bbox_world, glm::dvec2& A0, glm::dvec2& A1) {
		// calculate the local coordinate of A1
		glm::dvec2 a = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(A1, 1));
		
		// setup the initial parameters for optimization
		column_vector starting_point(2);
		column_vector lower_bound(2);
		column_vector upper_bound(2);
		starting_point(0, 0) = A1.x;
		starting_point(1, 0) = A1.y;
		lower_bound(0, 0) = bbox_world.minPt.x;
		lower_bound(1, 0) = bbox_world.minPt.y;
		upper_bound(0, 0) = bbox_world.maxPt.x;
		upper_bound(1, 0) = bbox_world.maxPt.y;

		double min_range = std::numeric_limits<double>::max();
		for (int i = 0; i < 2; i++) {
			min_range = std::min(min_range, upper_bound(i, 0) - lower_bound(i, 0));
		}

		try {
			find_min_bobyqa(SolverForSlider(poses), starting_point, 5, lower_bound, upper_bound, min_range * 0.19, min_range * 0.0001, 1000);

			A1.x = starting_point(0, 0);
			A1.y = starting_point(1, 0);

			// if the moving point is outside the valid region, discard it.
			if (!withinPolygon(linkage_region_pts, A1)) return false;
		}
		catch (std::exception& e) {
			//std::cout << e.what() << std::endl;
		}

		glm::dvec2 v1 = glm::dvec2(poses[1] * glm::dvec3(a, 1)) - A1;
		double l1 = glm::length(v1);
		v1 /= l1;

		for (int i = 2; i < poses.size(); i++) {
			glm::dvec2 A(poses[i] * glm::dvec3(a, 1));
			glm::dvec2 v = A - A1;
			double l = glm::length(v);

			// check the collinearity
			if (abs(crossProduct(v1, v / l)) > 0.01) return false;

			// check the order
			l = glm::dot(v1, v);
			if (l <= 0) return false;
			if (l <= l1) return false;
		}

		A0 = A1 - v1;

		// if the sampled point is outside the valid region, discard it.
		//if (!withinPolygon(linkage_region_pts, A0)) return false;

		return true;
	}

	Solution LinkageSynthesisRRRP::findBestSolution(const std::vector<glm::dmat3x3>& poses, std::vector<Solution>& solutions, const std::vector<glm::dvec2>& linkage_region_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, std::vector<Object25D>& fixed_body_pts, const Object25D& body_pts, bool rotatable_crank, bool avoid_branch_defect, double min_link_length, double position_error_weight, double orientation_error_weight, double linkage_location_weight, double smoothness_weight, double size_weight) {
		// select the best solution based on the objective function
		if (solutions.size() > 0) {
			double min_cost = std::numeric_limits<double>::max();
			int best = -1;
			for (int i = 0; i < solutions.size(); i++) {
				double position_error = solutions[i].position_error;
				double orientation_error = solutions[i].orientation_error;
				double linkage_location = solutions[i].dist;
				double tortuosity = tortuosityOfTrajectory(solutions[i].poses, solutions[i].points, body_pts);
				double size = glm::length(solutions[i].points[0] - solutions[i].points[2]) + glm::length(solutions[i].points[1] - solutions[i].points[3]) + glm::length(solutions[i].points[2] - solutions[i].points[3]);
				double cost = position_error * position_error_weight + orientation_error * orientation_error_weight + linkage_location * linkage_location_weight + tortuosity * smoothness_weight + size * size_weight;
				if (cost < min_cost) {
					min_cost = cost;
					best = i;
				}
			}

			return solutions[best];
		}
		else {
			return Solution({ { 0, 0 }, { 0, 2 }, { 2, 0 }, { 2, 2 }, { 4, 2 } }, 0, 0, 0, poses, {}, BBox());
		}
	}

	/**
	* Construct a linkage.
	*/
	Kinematics LinkageSynthesisRRRP::constructKinematics(const std::vector<glm::dvec2>& points, const std::vector<std::vector<int>>& zorder, const Object25D& body_pts, bool connect_joints, std::vector<Object25D>& fixed_body_pts) {
		kinematics::Kinematics kin;
		kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(0, true, points[0], zorder.size() == 3 ? zorder[2][0] : 1)));
		kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(1, true, points[1], zorder.size() == 3 ? zorder[2][1] : 1)));
		kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(2, false, points[2], zorder.size() == 3 ? zorder[2][0] : 1)));
		kin.diagram.addJoint(boost::shared_ptr<kinematics::SliderHinge>(new kinematics::SliderHinge(3, false, points[3], zorder.size() == 3 ? zorder[2][1] : 1)));
		kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(4, true, points[4], zorder.size() == 3 ? zorder[2][1] : 1)));
		kin.diagram.addLink(true, kin.diagram.joints[0], kin.diagram.joints[2], true, zorder.size() == 3 ? zorder[2][0] : 1);
		kin.diagram.addLink(false, { kin.diagram.joints[1], kin.diagram.joints[3], kin.diagram.joints[4] }, true, zorder.size() == 3 ? zorder[2][1] : 1);
		kin.diagram.addLink(false, kin.diagram.joints[2], kin.diagram.joints[3], false);

		// update the geometry
		updateBodies(kin, body_pts);
		if (connect_joints) {
			kin.diagram.connectJointsToBodies(fixed_body_pts, zorder);
		}

		// add the fixed rigid bodies
		for (int i = 0; i < fixed_body_pts.size(); i++) {
			kin.diagram.addBody(kin.diagram.joints[0], kin.diagram.joints[1], fixed_body_pts[i]);
		}

		return kin;
	}

	/**
	* update bodies.
	*/
	void LinkageSynthesisRRRP::updateBodies(Kinematics& kin, const Object25D& body_pts) {
		kin.diagram.bodies.clear();
		kin.diagram.addBody(kin.diagram.joints[2], kin.diagram.joints[3], body_pts);
	}

	bool LinkageSynthesisRRRP::checkHardConstraints(std::vector<glm::dvec2>& points, const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& linkage_region_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, std::vector<Object25D>& fixed_body_pts, const Object25D& body_pts, bool rotatable_crank, bool avoid_branch_defect, double min_link_length, std::vector<std::vector<int>>& zorder) {
		glm::dvec2 slider_dir = points[3] - points[1];

		// check hard constraints
		if (glm::length(points[0] - points[1]) < min_link_length) return false;
		if (glm::length(points[2] - points[3]) < min_link_length) return false;

		if (rotatable_crank && checkRotatableCrankDefect(points)) return false;
		if (avoid_branch_defect && checkBranchDefect(poses, points)) return false;
		if (checkCircuitDefect(poses, points)) return false;

		// collision check
		glm::dvec2 slider_end_pos1, slider_end_pos2;
		if (checkCollision(poses, points, fixed_body_pts, body_pts[0], linkage_avoidance_pts, slider_end_pos1, slider_end_pos2, 2)) return false;

		// locate the two endpoints of the bar
		points[1] = slider_end_pos1 - slider_dir * 2.0;
		points[4] = slider_end_pos2 + slider_dir * 2.0;
		if (!withinPolygon(linkage_region_pts, points[1])) return false;
		if (!withinPolygon(linkage_region_pts, points[4])) return false;

		// record collision between connectors
		Kinematics kin = recordCollisionForConnectors(poses, points, fixed_body_pts, body_pts[0]);

		// determine the z-order of links and connectors
		try {
			zorder = ZOrder::zorderConnectors(kin.diagram.connectors);
		}
		catch (char* ex) {
			return false;
		}
		
		return true;
	}

	/**
	* Return the RRRP linkage type.
	*
	* 0 -- rotatable crank
	* 1 -- 0-rocker
	* 2 -- pi-rocker
	* 3 -- rocker
	*/
	int LinkageSynthesisRRRP::getType(const std::vector<glm::dvec2>& points) {
		// obtain the vectors, u (x axis) and v (y axis)
		glm::dvec2 u = points[3] - points[1];
		u /= glm::length(u);

		glm::dvec2 v(-u.y, u.x);
		if (glm::dot(points[0] - points[1], v) < 0) {
			u = -u;
			v = -v;
		}

		// calculate each length
		double e = glm::dot(points[0] - points[1], v);
		double r = glm::length(points[2] - points[0]);
		double l = glm::length(points[3] - points[2]);

		// calculate S1 and S2
		double S1 = l - r + e;
		double S2 = l - r - e;

		// judge the type of the RRRP linkage
		if (S1 >= 0 && S2 >= 0) return 0;
		else if (S1 >= 0 && S2 < 0) {
			// HACK to differentiate 0-rocker from pi-rocker
			if (v.y >= 0) return 1;
			else return 2;
		}
		//else if (S1 < 0 && S2 >= 0) return 2;
		else return 3;
	}

	/**
	* Check if the linkage has rotatable crank defect.
	* If the crank is not fully rotatable, true is returned.
	*/
	bool LinkageSynthesisRRRP::checkRotatableCrankDefect(const std::vector<glm::dvec2>& points) {
		int linkage_type = getType(points);

		if (linkage_type == 0) {
			return false;
		}
		else {
			return true;
		}
	}

	bool LinkageSynthesisRRRP::checkOrderDefect(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points) {
		return false;
	}

	bool LinkageSynthesisRRRP::checkBranchDefect(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points) {
		int type = getType(points);

		// rotatable crank always does not have a branch defect
		if (type == 0) return false;

		// obtain the vectors, u (x axis) and v (y axis)
		glm::dvec2 u = points[3] - points[1];
		u /= glm::length(u);

		glm::dvec2 v(-u.y, u.x);
		if (glm::dot(points[0] - points[1], v) < 0) {
			u = -u;
			v = -v;
		}

		int orig_sign = 1;

		// calculate the local coordinates of the circle points
		glm::dvec2 q2 = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(points[2], 1));
		glm::dvec2 q3 = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(points[3], 1));

		for (int i = 0; i < poses.size(); i++) {
			// calculate the coordinates of the circle point of the driving/driven cranks in the world coordinate system
			glm::dvec2 P2 = glm::dvec2(poses[i] * glm::dvec3(q2, 1));
			glm::dvec2 P3 = glm::dvec2(poses[i] * glm::dvec3(q3, 1));

			// calculate the sign of the dot product of L and u
			if (i == 0) {
				orig_sign = glm::dot(P3 - P2, u) >= 0 ? 1 : -1;
			}
			else {
				int sign = glm::dot(P3 - P2, u) >= 0 ? 1 : -1;
				if (sign != orig_sign) return true;
			}
		}

		return false;
	}

	bool LinkageSynthesisRRRP::checkCircuitDefect(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points) {
		int type = getType(points);

		// 0-rocker and pi-rocker always do not have a branch defect
		if (type == 1 || type == 2) return false;

		// obtain the vectors, u (x axis) and v (y axis)
		glm::dvec2 u = points[3] - points[1];
		u /= glm::length(u);

		glm::dvec2 v(-u.y, u.x);
		if (glm::dot(points[0] - points[1], v) < 0) {
			u = -u;
			v = -v;
		}

		int orig_sign = 1;

		// calculate the local coordinates of the circle points
		glm::dvec2 q2 = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(points[2], 1));
		glm::dvec2 q3 = glm::dvec2(glm::inverse(poses[0]) * glm::dvec3(points[3], 1));

		for (int i = 0; i < poses.size(); i++) {
			// calculate the coordinates of the circle point of the driving/driven cranks in the world coordinate system
			glm::dvec2 P2 = glm::dvec2(poses[i] * glm::dvec3(q2, 1));
			glm::dvec2 P3 = glm::dvec2(poses[i] * glm::dvec3(q3, 1));

			// calculate the sign of the dot product of L and u
			if (i == 0) {
				if (type == 0) {
					orig_sign = glm::dot(P3 - P2, u) >= 0 ? 1 : -1;
				}
				else {
					orig_sign = glm::dot(P2 - points[0], u) >= 0 ? 1 : -1;
				}
			}
			else {
				int sign;
				if (type == 0) {
					sign = glm::dot(P3 - P2, u) >= 0 ? 1 : -1;
				}
				else {
					sign = glm::dot(P2 - points[0], u) >= 0 ? 1 : -1;
				}
				if (sign != orig_sign) return true;
			}
		}

		return false;
	}

	bool LinkageSynthesisRRRP::checkCollision(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points, std::vector<Object25D> fixed_body_pts, const Object25D& body_pts, const std::vector<glm::dvec2>& linkage_avoidance_pts, glm::dvec2& slider_end_pos1, glm::dvec2& slider_end_pos2, int collision_check_type) {
		kinematics::Kinematics kinematics = constructKinematics(points, {}, { body_pts }, (collision_check_type == 1 || collision_check_type == 3), fixed_body_pts);
		kinematics.diagram.initialize();

		// set the initial point of slider and direction
		glm::dvec2 orig_slider_pos = points[3];
		glm::dvec2 slider_dir = glm::normalize(points[3] - points[1]);
		slider_end_pos1 = points[3];
		slider_end_pos2 = points[3];
		double slider_min_dist = 0;
		double slider_max_dist = 0;


		// calculate the rotational angle of the driving crank for 1st, 2nd, and last poses
		// i.e., angles[0] = first pose, angles[1] = second pose, angles[2] = last pose
		std::vector<double> angles(3);
		glm::dvec2 w(glm::inverse(poses[0]) * glm::dvec3(points[2], 1));
		for (int i = 0; i < 2; i++) {
			glm::dvec2 W = glm::dvec2(poses[i] * glm::dvec3(w, 1));
			angles[i] = atan2(W.y - points[0].y, W.x - points[0].x);
		}
		{
			glm::dvec2 W = glm::dvec2(poses.back() * glm::dvec3(w, 1));
			angles[2] = atan2(W.y - points[0].y, W.x - points[0].x);
		}

		// order the angles based on their signs
		int type = 0;
		if (angles[0] < 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] >= angles[1]) {
			type = 1;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] < angles[2]) {
			type = 2;
			angles[1] -= M_PI * 2;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] >= angles[2]) {
			type = 3;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] >= 0 && angles[1] >= angles[2]) {
			type = 4;
			angles[1] -= M_PI * 2;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] < 0 && angles[1] < angles[2]) {
			type = 5;
			angles[1] += M_PI * 2;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] < angles[2]) {
			type = 6;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] >= angles[2]) {
			type = 7;
			angles[1] += M_PI * 2;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] < angles[1]) {
			type = 8;
			angles[2] += M_PI * 2;
		}

		if (angles[2] < angles[0]) {
			kinematics.invertSpeed();
		}

		// initialize the visited flag
		std::vector<bool> visited(angles.size(), false);
		visited[0] = true;
		int unvisited = 2;

		// run forward until collision is deteted or all the poses are reached
		while (true) {
			try {
				kinematics.stepForward(collision_check_type, false);
				double dist = glm::dot(kinematics.diagram.joints[3]->pos - orig_slider_pos, slider_dir);
				if (dist > slider_max_dist) {
					slider_max_dist = dist;
					slider_end_pos2 = kinematics.diagram.joints[3]->pos;
				}
				else if (dist < slider_min_dist) {
					slider_min_dist = dist;
					slider_end_pos1 = kinematics.diagram.joints[3]->pos;
				}
			}
			catch (char* ex) {
				// if only some of the poses are reached before collision, the collision is detected.
				kinematics.clear();
				return true;
			}

			// check if the joints are within the linkage avoidance region
			for (int i = 0; i < kinematics.diagram.joints.size(); i++) {
				if (withinPolygon(linkage_avoidance_pts, kinematics.diagram.joints[i]->pos)) return true;
			}

			// calculate the angle of the driving crank
			double angle = atan2(kinematics.diagram.joints[2]->pos.y - points[0].y, kinematics.diagram.joints[2]->pos.x - points[0].x);

			// convert the sign of the angle
			if (type == 1 && angle > 0) {
				angle -= M_PI * 2;
			}
			else if (type == 2 && angle > angles[0]) {
				angle -= M_PI * 2;
			}
			else if (type == 3 && angle < angles[0]) {
				angle += M_PI * 2;
			}
			else if (type == 4 && angle > 0) {
				angle -= M_PI * 2;
			}
			else if (type == 5 && angle < 0) {
				angle += M_PI * 2;
			}
			else if (type == 6 && angle > angles[0]) {
				angle -= M_PI * 2;
			}
			else if (type == 7 && angle < angles[0]) {
				angle += M_PI * 2;
			}
			else if (type == 8 && angle < 0) {
				angle += M_PI * 2;
			}

			// check if the poses are reached
			for (int i = 0; i < angles.size(); i++) {
				if (visited[i]) continue;

				if (angles[2] >= angles[0]) {
					if (angle >= angles[i]) {
						visited[i] = true;
						unvisited--;
					}
				}
				else {
					if (angle <= angles[i]) {
						visited[i] = true;
						unvisited--;
					}
				}
			}

			// if all the poses are reached without collision, no collision is detected.
			if (unvisited == 0) {
				kinematics.clear();
				return false;
			}
		}

		kinematics.clear();
		return false;
	}

	Kinematics LinkageSynthesisRRRP::recordCollisionForConnectors(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points, std::vector<Object25D> fixed_body_pts, const Object25D& body_pts) {
		Kinematics kinematics = constructKinematics(points, {}, { body_pts }, true, fixed_body_pts);
		kinematics.diagram.initialize();

		// calculate the rotational angle of the driving crank for 1st, 2nd, and last poses
		// i.e., angles[0] = first pose, angles[1] = second pose, angles[2] = last pose
		std::vector<double> angles(3);
		glm::dvec2 w(glm::inverse(poses[0]) * glm::dvec3(points[2], 1));
		for (int i = 0; i < 2; i++) {
			glm::dvec2 W = glm::dvec2(poses[i] * glm::dvec3(w, 1));
			angles[i] = atan2(W.y - points[0].y, W.x - points[0].x);
		}
		{
			glm::dvec2 W = glm::dvec2(poses.back() * glm::dvec3(w, 1));
			angles[2] = atan2(W.y - points[0].y, W.x - points[0].x);
		}

		// order the angles based on their signs
		int type = 0;
		if (angles[0] < 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] >= angles[1]) {
			type = 1;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] < angles[2]) {
			type = 2;
			angles[1] -= M_PI * 2;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] >= angles[2]) {
			type = 3;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] >= 0 && angles[1] >= angles[2]) {
			type = 4;
			angles[1] -= M_PI * 2;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] < 0 && angles[1] < angles[2]) {
			type = 5;
			angles[1] += M_PI * 2;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] < angles[2]) {
			type = 6;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] >= angles[2]) {
			type = 7;
			angles[1] += M_PI * 2;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] < angles[1]) {
			type = 8;
			angles[2] += M_PI * 2;
		}

		if (angles[2] < angles[0]) {
			kinematics.invertSpeed();
		}

		// initialize the visited flag
		std::vector<bool> visited(angles.size(), false);
		visited[0] = true;
		int unvisited = 2;

		// check the collision at the initial pose
		kinematics.diagram.recordCollisionForConnectors();

		// run forward until collision is deteted or all the poses are reached
		while (true) {
			try {
				kinematics.stepForward(3, false);
			}
			catch (char* ex) {
				// if only some of the poses are reached before collision, the collision is detected.
				return kinematics;
			}

			// calculate the angle of the driving crank
			double angle = atan2(kinematics.diagram.joints[2]->pos.y - points[0].y, kinematics.diagram.joints[2]->pos.x - points[0].x);

			// convert the sign of the angle
			if (type == 1 && angle > 0) {
				angle -= M_PI * 2;
			}
			else if (type == 2 && angle > angles[0]) {
				angle -= M_PI * 2;
			}
			else if (type == 3 && angle < angles[0]) {
				angle += M_PI * 2;
			}
			else if (type == 4 && angle > 0) {
				angle -= M_PI * 2;
			}
			else if (type == 5 && angle < 0) {
				angle += M_PI * 2;
			}
			else if (type == 6 && angle > angles[0]) {
				angle -= M_PI * 2;
			}
			else if (type == 7 && angle < angles[0]) {
				angle += M_PI * 2;
			}
			else if (type == 8 && angle < 0) {
				angle += M_PI * 2;
			}

			// check if the poses are reached
			for (int i = 0; i < angles.size(); i++) {
				if (visited[i]) continue;

				if (angles[2] >= angles[0]) {
					if (angle >= angles[i]) {
						visited[i] = true;
						unvisited--;
					}
				}
				else {
					if (angle <= angles[i]) {
						visited[i] = true;
						unvisited--;
					}
				}
			}

			// if all the poses are reached without collision, no collision is detected.
			if (unvisited == 0) {
				return kinematics;
			}
		}

		return kinematics;
	}

	double LinkageSynthesisRRRP::tortuosityOfTrajectory(const std::vector<glm::dmat3x3>& poses, const std::vector<glm::dvec2>& points, const Object25D& body_pts) {
		// calculate the local coordinates of the body points
		glm::dmat3x3 inv_pose0 = glm::inverse(poses[0]);
		std::vector<glm::dvec2> body_pts_local(body_pts.polygons[0].points.size());
		for (int i = 0; i < body_pts.polygons[0].points.size(); i++) {
			body_pts_local[i] = glm::dvec2(inv_pose0 * glm::dvec3(body_pts.polygons[0].points[i], 1));
		}

		// calculate the length of the motion using straight lines between poses
		double length_of_straight = 0.0;
		std::vector<glm::dvec2> prev_body_pts = body_pts.polygons[0].points;
		for (int i = 1; i < poses.size(); i++) {
			std::vector<glm::dvec2> next_body_pts(body_pts.polygons[0].points.size());
			for (int k = 0; k < body_pts.polygons[0].points.size(); k++) {
				next_body_pts[k] = glm::dvec2(poses[i] * glm::dvec3(body_pts_local[k], 1));
				length_of_straight += glm::length(next_body_pts[k] - prev_body_pts[k]);
			}
			prev_body_pts = next_body_pts;
		}

		// create a kinematics
		kinematics::Kinematics kinematics = constructKinematics(points, {}, { body_pts }, false);
		kinematics.diagram.initialize();

		// initialize the trajectory of the moving body
		prev_body_pts = body_pts.polygons[0].points;
		double length_of_trajectory = 0.0;

		// calculate the rotational angle of the driving crank for 1st, 2nd, and last poses
		// i.e., angles[0] = first pose, angles[1] = second pose, angles[2] = last pose
		std::vector<double> angles(3);
		glm::dvec2 w(glm::inverse(poses[0]) * glm::dvec3(points[2], 1));
		for (int i = 0; i < 2; i++) {
			glm::dvec2 W = glm::dvec2(poses[i] * glm::dvec3(w, 1));
			angles[i] = atan2(W.y - points[0].y, W.x - points[0].x);
		}
		{
			glm::dvec2 W = glm::dvec2(poses.back() * glm::dvec3(w, 1));
			angles[2] = atan2(W.y - points[0].y, W.x - points[0].x);
		}

		// order the angles based on their signs
		int type = 0;
		if (angles[0] < 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] >= angles[1]) {
			type = 1;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] < angles[2]) {
			type = 2;
			angles[1] -= M_PI * 2;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] >= angles[2]) {
			type = 3;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] < 0 && angles[1] >= 0 && angles[2] >= 0 && angles[1] >= angles[2]) {
			type = 4;
			angles[1] -= M_PI * 2;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] < 0 && angles[1] < angles[2]) {
			type = 5;
			angles[1] += M_PI * 2;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] < angles[2]) {
			type = 6;
			angles[2] -= M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] < 0 && angles[2] >= 0 && angles[0] >= angles[2]) {
			type = 7;
			angles[1] += M_PI * 2;
			angles[2] += M_PI * 2;
		}
		else if (angles[0] >= 0 && angles[1] >= 0 && angles[2] < 0 && angles[0] < angles[1]) {
			type = 8;
			angles[2] += M_PI * 2;
		}

		if (angles[2] < angles[0]) {
			kinematics.invertSpeed();
		}

		// initialize the visited flag
		std::vector<bool> visited(angles.size(), false);
		visited[0] = true;
		int unvisited = 2;

		// run forward until collision is deteted or all the poses are reached
		while (true) {
			try {
				kinematics.stepForward(2, false);
			}
			catch (char* ex) {
				// if only some of the poses are reached before collision, the collision is detected.
				kinematics.clear();
				return length_of_trajectory / length_of_straight;
			}

			// calculate the angle of the driving crank
			double angle = atan2(kinematics.diagram.joints[2]->pos.y - points[0].y, kinematics.diagram.joints[2]->pos.x - points[0].x);

			// update the lengths of the trajectory of the moving body
			std::vector<glm::dvec2> next_body_pts = kinematics.diagram.bodies[0]->getActualPoints()[0];
			for (int i = 0; i < next_body_pts.size(); i++) {
				double length = glm::length(next_body_pts[i] - prev_body_pts[i]);
				length_of_trajectory += length;
				prev_body_pts[i] = next_body_pts[i];
			}

			// convert the sign of the angle
			if (type == 1 && angle > 0) {
				angle -= M_PI * 2;
			}
			else if (type == 2 && angle > angles[0]) {
				angle -= M_PI * 2;
			}
			else if (type == 3 && angle < angles[0]) {
				angle += M_PI * 2;
			}
			else if (type == 4 && angle > 0) {
				angle -= M_PI * 2;
			}
			else if (type == 5 && angle < 0) {
				angle += M_PI * 2;
			}
			else if (type == 6 && angle > angles[0]) {
				angle -= M_PI * 2;
			}
			else if (type == 7 && angle < angles[0]) {
				angle += M_PI * 2;
			}
			else if (type == 8 && angle < 0) {
				angle += M_PI * 2;
			}

			// check if the poses are reached
			for (int i = 0; i < angles.size(); i++) {
				if (visited[i]) continue;

				if (angles[2] >= angles[0]) {
					if (angle >= angles[i]) {
						visited[i] = true;
						unvisited--;
					}
				}
				else {
					if (angle <= angles[i]) {
						visited[i] = true;
						unvisited--;
					}
				}
			}

			// if all the poses are reached without collision, no collision is detected.
			if (unvisited == 0) {
				kinematics.clear();
				return length_of_trajectory / length_of_straight;
			}
		}

		kinematics.clear();
		return length_of_trajectory / length_of_straight;
	}

}