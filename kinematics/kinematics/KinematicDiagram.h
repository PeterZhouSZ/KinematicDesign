#pragma once

#include <vector>
#include <boost/shared_ptr.hpp>
#include <QMap>

#include "Joint.h"
#include "Link.h"
#include "BodyGeometry.h"
#include "Vertex.h"

namespace kinematics {

	class Options {
	public:
		static Options* instance;
		float gap;
		float link_width;
		float link_depth;
		float hole_radius;
		float joint_radius;
		float joint_cap_radius1;
		float joint_cap_radius2;
		float joint_cap_depth;
		float slider_bar_width;
		float slider_bar_depth;
		float slider_width;
		float slider_depth;

	protected:
		Options() {
			gap = 0.01f;
			link_width = 1.0f;
			link_depth = 0.3f;
			hole_radius = 0.26f;
			joint_radius = 0.25f;
			joint_cap_radius1 = 0.23f;
			joint_cap_radius2 = 0.28f;
			joint_cap_depth = 0.15f;
			slider_bar_width = 0.6f;
			slider_bar_depth = 0.3f;
			slider_width = 1.0f;
			slider_depth = 0.5f;
		}

	public:
		static Options* getInstance() {
			if (!instance) instance = new Options();
			return instance;
		}
	};
	
	static Options* options = Options::getInstance();

	class KinematicDiagram {
	public:
		QMap<int, boost::shared_ptr<Joint>> joints;
		QMap<int, boost::shared_ptr<Link>> links;
		std::vector<boost::shared_ptr<BodyGeometry>> bodies;

	public:
		KinematicDiagram();
		~KinematicDiagram();

		KinematicDiagram clone() const;
		void clear();
		void initialize();
		void addJoint(boost::shared_ptr<Joint> joint);
		void setJointToLink(boost::shared_ptr<Joint> joint, boost::shared_ptr<Link> link);
		boost::shared_ptr<Link> newLink();
		boost::shared_ptr<Link> newLink(bool driver, bool actual_link = true, double z = 0);
		boost::shared_ptr<Link> addLink(boost::shared_ptr<Joint> joint1, boost::shared_ptr<Joint> joint2, bool actual_link = true, double z = 0);
		boost::shared_ptr<Link> addLink(bool driver, boost::shared_ptr<Joint> joint1, boost::shared_ptr<Joint> joint2, bool actual_link = true, double z = 0);
		boost::shared_ptr<Link> addLink(std::vector<boost::shared_ptr<Joint>> joints, bool actual_link = true, double z = 0);
		boost::shared_ptr<Link> addLink(bool driver, std::vector<boost::shared_ptr<Joint>> joints, bool actual_link = true, double z = 0);
		void addBody(boost::shared_ptr<Joint> joint1, boost::shared_ptr<Joint> joint2, const Object25D& polygons);
		void addPolygonToBody(int body_id, const Polygon25D& polygon);
		void connectJointsToBodies(std::vector<Object25D>& fixed_body_pts);
		void load(const QString& filename);
		void save(const QString& filename);
		void updateBodyAdjacency();
		bool isCollided() const;
		void draw(QPainter& painter, const QPointF& origin, float scale, bool show_bodies, bool show_links) const;
	};

}