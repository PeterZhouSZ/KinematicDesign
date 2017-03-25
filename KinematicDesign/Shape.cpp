#include "Shape.h"
#include <QImage>
#include "Utils.h"

namespace canvas {

	QImage Shape::rotation_marker = QImage("resources/rotation_marker.png").scaled(16, 16);

	Shape::Shape() {
		selected = false;
		currently_drawing = true;
		//angle = 0;
		model_mat = glm::dmat4x4();
	}
	
	Shape::~Shape() {
	}

	void Shape::select() {
		selected = true;
	}

	void Shape::unselect() {
		selected = false;
	}

	bool Shape::isSelected() const {
		return selected;
	}

	void Shape::complete() {
		currently_drawing = false;
	}

	void Shape::translate(const glm::dvec2& vec) {
		model_mat = glm::translate(glm::dmat4x4(), glm::dvec3(vec, 0)) * model_mat;

		QTransform tr;
		tr.translate(vec.x, vec.y);
		transform = transform * tr;
	}

	void Shape::rotate(double angle) {
		glm::dvec2 c = boundingBox().center();

		model_mat = glm::translate(model_mat, glm::dvec3(c, 0));
		model_mat = glm::rotate(model_mat, angle, glm::dvec3(0, 0, 1));
		model_mat = glm::translate(model_mat, glm::dvec3(-c, 0));
		transform.translate(c.x, c.y);
		transform.rotate(angle / kinematics::M_PI * 180);
		transform.translate(-c.x, -c.y);
	}

	glm::dvec2 Shape::getOrigin() const{
		return origin;
	}

	/*
	double Shape::getAngle() const {
		return angle;
	}
	*/

	glm::dvec2 Shape::getCenter() const {
		return boundingBox().center();
	}

	glm::dvec2 Shape::getRotationMarkerPosition() const {
		BoundingBox bbox = boundingBox();

		return glm::dvec2(bbox.center().x, bbox.minPt.y - 10);
	}
	
	glm::dvec2 Shape::localCoordinate(const glm::dvec2& point) const {
		return glm::dvec2(glm::inverse(model_mat) * glm::dvec4(point, 0, 1));
		/*
		glm::dvec2 c = boundingBox().center();
		return kinematics::rotatePoint(point - origin - c, -angle) + c;
		*/
	}

	glm::dvec2 Shape::worldCoordinate(const glm::dvec2& point) const {
		return glm::dvec2(model_mat * glm::dvec4(point, 0, 1));
		/*
		glm::dvec2 c = boundingBox().center();
		return kinematics::rotatePoint(point - c, angle) + origin + c;
		*/
	}
}