#pragma once

#include "Shape.h"

namespace canvas {
	class Point;

	class Polygon : public Shape {
	private:
		std::vector<glm::dvec2> points;
		glm::dvec2 current_point;

	public:
		Polygon();
		Polygon(const glm::dvec2& point);
		~Polygon();

		boost::shared_ptr<Shape> clone();
		void draw(QPainter& painter) const;
		void addPoint(const glm::dvec2& point);
		void updateByNewPoint(const glm::dvec2& point);
		bool hit(const glm::dvec2& point) const;
		//void translate(const glm::dvec2& vec);
		void resize(const glm::dvec2& scale, int resize_type);
		BoundingBox boundingBox() const;
	};

}