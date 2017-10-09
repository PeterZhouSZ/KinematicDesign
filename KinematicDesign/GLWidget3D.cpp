#include "GLWidget3D.h"
#include "MainWindow.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include "GLUtils.h"
#include <QDir>
#include <QTextStream>
#include <QDate>
#include <iostream>
#include <QProcess>
#include "Rectangle.h"
#include "Circle.h"
#include "Polygon.h"
#include "STLExporter.h"
#include "SCADExporter.h"

GLWidget3D::GLWidget3D(MainWindow *parent) : QGLWidget(QGLFormat(QGL::SampleBuffers)) {
	this->mainWin = parent;
	ctrlPressed = false;
	shiftPressed = false;

	first_paint = true;
	front_faced = true;

	mode = MODE_SELECT;
	layers.resize(2);
	layer_id = 0;
	current_shape.reset();
	operation.reset();

	linkage_type = LINKAGE_4R;
	animation_timer = NULL;
	collision_check = true;
	show_solutions = false;

	// This is necessary to prevent the screen overdrawn by OpenGL
	setAutoFillBackground(false);

	// light direction for shadow mapping
	light_dir = glm::normalize(glm::vec3(-4, -5, -8));

	// model/view/projection matrices for shadow mapping
	glm::mat4 light_pMatrix = glm::ortho<float>(-100, 100, -100, 100, 0.1, 200);
	glm::mat4 light_mvMatrix = glm::lookAt(-light_dir * 50.0f, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	light_mvpMatrix = light_pMatrix * light_mvMatrix;

	// spot light
	spot_light_pos = glm::vec3(2, 2.5, 8);
}

/**
* Draw the scene.
*/
void GLWidget3D::drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(true);

	renderManager.renderAll();
}

void GLWidget3D::render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 1: Render to texture
	glUseProgram(renderManager.programs["pass1"]);

	glBindFramebuffer(GL_FRAMEBUFFER, renderManager.fragDataFB);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager.fragDataTex[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, renderManager.fragDataTex[1], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, renderManager.fragDataTex[2], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, renderManager.fragDataTex[3], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderManager.fragDepthTex, 0);

	// Set the list of draw buffers.
	GLenum DrawBuffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(4, DrawBuffers); // "3" is the size of DrawBuffers
	// Always check that our framebuffer is ok
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("+ERROR: GL_FRAMEBUFFER_COMPLETE false\n");
		exit(0);
	}

	glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["pass1"], "mvpMatrix"), 1, false, &camera.mvpMatrix[0][0]);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "lightDir"), light_dir.x, light_dir.y, light_dir.z);
	glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["pass1"], "light_mvpMatrix"), 1, false, &light_mvpMatrix[0][0]);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "spotLightPos"), spot_light_pos.x, spot_light_pos.y, spot_light_pos.z);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "cameraPos"), camera.pos.x, camera.pos.y, camera.pos.z);

	glUniform1i(glGetUniformLocation(renderManager.programs["pass1"], "shadowMap"), 6);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, renderManager.shadow.textureDepth);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	drawScene();

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 2: Create AO
	if (renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
		glUseProgram(renderManager.programs["ssao"]);
		glBindFramebuffer(GL_FRAMEBUFFER, renderManager.fragDataFB_AO);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager.fragAOTex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderManager.fragDepthTex_AO, 0);
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Always check that our framebuffer is ok
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			printf("++ERROR: GL_FRAMEBUFFER_COMPLETE false\n");
			exit(0);
		}

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["ssao"], "pixelSize"), 2.0f / this->width(), 2.0f / this->height());

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex0"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex1"), 2);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "noiseTex"), 7);
		glActiveTexture(GL_TEXTURE7);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragNoiseTex);

		{
			glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["ssao"], "mvpMatrix"), 1, false, &camera.mvpMatrix[0][0]);
			glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["ssao"], "pMatrix"), 1, false, &camera.pMatrix[0][0]);
		}

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "uKernelSize"), renderManager.uKernelSize);
		glUniform3fv(glGetUniformLocation(renderManager.programs["ssao"], "uKernelOffsets"), renderManager.uKernelOffsets.size(), (const GLfloat*)renderManager.uKernelOffsets.data());

		glUniform1f(glGetUniformLocation(renderManager.programs["ssao"], "uPower"), renderManager.uPower);
		glUniform1f(glGetUniformLocation(renderManager.programs["ssao"], "uRadius"), renderManager.uRadius);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}
	else if (renderManager.renderingMode == RenderManager::RENDERING_MODE_LINE || renderManager.renderingMode == RenderManager::RENDERING_MODE_HATCHING || renderManager.renderingMode == RenderManager::RENDERING_MODE_SKETCHY) {
		glUseProgram(renderManager.programs["line"]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["line"], "pixelSize"), 1.0f / this->width(), 1.0f / this->height());
		glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["line"], "pMatrix"), 1, false, &camera.pMatrix[0][0]);
		if (renderManager.renderingMode == RenderManager::RENDERING_MODE_HATCHING) {
			glUniform1i(glGetUniformLocation(renderManager.programs["line"], "useHatching"), 1);
		}
		else {
			glUniform1i(glGetUniformLocation(renderManager.programs["line"], "useHatching"), 0);
		}

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex0"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex1"), 2);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex3"), 4);
		glActiveTexture(GL_TEXTURE4);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[3]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "hatchingTexture"), 5);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, renderManager.hatchingTextures);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}
	else if (renderManager.renderingMode == RenderManager::RENDERING_MODE_CONTOUR) {
		glUseProgram(renderManager.programs["contour"]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["contour"], "pixelSize"), 1.0f / this->width(), 1.0f / this->height());

		glUniform1i(glGetUniformLocation(renderManager.programs["contour"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Blur

	if (renderManager.renderingMode == RenderManager::RENDERING_MODE_BASIC || renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUseProgram(renderManager.programs["blur"]);
		glUniform2f(glGetUniformLocation(renderManager.programs["blur"], "pixelSize"), 2.0f / this->width(), 2.0f / this->height());
		//printf("pixelSize loc %d\n", glGetUniformLocation(vboRenderManager.programs["blur"], "pixelSize"));

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex0"), 1);//COLOR
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex1"), 2);//NORMAL
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		/*glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);*/

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex3"), 4);//AO
		glActiveTexture(GL_TEXTURE4);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragAOTex);

		if (renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
			glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "ssao_used"), 1); // ssao used
		}
		else {
			glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "ssao_used"), 0); // no ssao
		}

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);

	}

	// REMOVE
	glActiveTexture(GL_TEXTURE0);
}

void GLWidget3D::clear() {
	for (int i = 0; i < layers.size(); ++i) {
		layers[i].clear();
	}
	selected_shape.reset();

	// select 1st layer
	setLayer(0);

	// clear the kinematic data
	kinematics.clear();
	solutions.clear();
	selectedJoint = std::make_pair(-1, -1);

	// update 3D geometry
	update3DGeometry();

	update();
}

void GLWidget3D::selectAll() {
	layers[layer_id].selectAll();
	mode = MODE_SELECT;
	update();
}

void GLWidget3D::unselectAll() {
	layers[layer_id].unselectAll();
	current_shape.reset();
	update();
}

void GLWidget3D::deleteSelectedShapes() {
	for (int i = layers[layer_id].shapes.size() - 1; i >= 0; --i) {
		if (layers[layer_id].shapes[i]->isSelected()) {
			for (int l = 0; l < layers.size(); l++) {
				layers[l].shapes.erase(layers[l].shapes.begin() + i);
			}
		}
	}

	// update 3D geometry
	update3DGeometry();

	current_shape.reset();
	update();
}

void GLWidget3D::undo() {
	try {
		layers = history.undo();

		// update 3D geometry
		update3DGeometry();

		update();
	}
	catch (char* ex) {
	}
}

void GLWidget3D::redo() {
	try {
		layers = history.redo();

		// update 3D geometry
		update3DGeometry();

		update();
	}
	catch (char* ex) {
	}
}

void GLWidget3D::copySelectedShapes() {
	layers[layer_id].copySelectedShapes(copied_shapes);
}

void GLWidget3D::pasteCopiedShapes() {
	layers[layer_id].pasteCopiedShapes(copied_shapes);

	// update 3D geometry
	update3DGeometry();

	current_shape.reset();
	mode = MODE_SELECT;
	update();
}


void GLWidget3D::setMode(int mode) {
	if (this->mode != mode) {
		this->mode = mode;

		// clear
		unselectAll();
		selectedJoint = std::make_pair(-1, -1);

		update();
	}
}

void GLWidget3D::addLayer() {
	layers.push_back(layers.back().clone());
	setLayer(layers.size() - 1);
}

void GLWidget3D::insertLayer() {
	layers.insert(layers.begin() + layer_id, layers[layer_id].clone());
	setLayer(layer_id);
}

void GLWidget3D::deleteLayer() {
	// we assume that there must be at least two layers.
	if (layers.size() <= 2) return;

	layers.erase(layers.begin() + layer_id);
	if (layer_id >= layers.size()) {
		layer_id--;
	}
	setLayer(layer_id);
}

void GLWidget3D::setLayer(int layer_id) {
	layers[this->layer_id].unselectAll();
	this->layer_id = layer_id;
	current_shape.reset();

	// update 3D geometry
	update3DGeometry();

	// change the mode to SELECT
	setMode(MODE_SELECT);

	update();
}


void GLWidget3D::open(const QString& filename) {
	QFile file(filename);
	if (!file.open(QFile::ReadOnly | QFile::Text)) throw "File cannot open.";

	// if the animation is running, stop it.
	if (animation_timer) {
		stop();
	}

	QDomDocument doc;
	doc.setContent(&file);

	QDomElement root = doc.documentElement();
	if (root.tagName() != "design")	throw "Invalid file format.";

	// clear the data
	layers.clear();
	selected_shape.reset();
	mode = MODE_SELECT;

	QDomNode layer_node = root.firstChild();
	while (!layer_node.isNull()) {
		if (layer_node.toElement().tagName() == "layer") {
			canvas::Layer layer;
			layer.load(layer_node.toElement());
			layers.push_back(layer);
		}

		layer_node = layer_node.nextSibling();
	}

	// select 1st layer to display
	layer_id = 0;

	// update 3D geometry
	update3DGeometry();

	// no currently drawing shape
	current_shape.reset();

	// clear the kinematic data
	kinematics.clear();
	solutions.clear();

	// update the layer menu based on the loaded data
	mainWin->initLayerMenu(layers.size());

	update();
}

void GLWidget3D::save(const QString& filename) {
	QFile file(filename);
	if (!file.open(QFile::WriteOnly)) throw "File cannot open.";

	QDomDocument doc;

	// set root node
	QDomElement root = doc.createElement("design");
	root.setAttribute("author", "Gen Nishida");
	root.setAttribute("version", "1.0");
	root.setAttribute("date", QDate::currentDate().toString("MM/dd/yyyy"));
	doc.appendChild(root);

	// write layers
	for (int i = 0; i < layers.size(); ++i) {
		QDomElement layer_node = layers[i].toXml(doc);
		root.appendChild(layer_node);
	}

	QTextStream out(&file);
	doc.save(out, 4);
}

void GLWidget3D::saveSTL(const QString& dirname) {
	for (int i = 0; i < kinematics.size(); i++) {
		// generate geometry of rigid bodies
		for (int j = 0; j < kinematics[i].diagram.bodies.size(); j++) {
			std::vector<Vertex> vertices;
			
			int N = kinematics[i].diagram.bodies[j]->size();
			for (int k = 0; k < N; k++) {
				std::vector<glm::dvec2> pts = kinematics[i].diagram.bodies[j]->getActualPoints(k);
				std::vector<glm::dvec2> pts2 = kinematics[i].diagram.bodies[j]->getActualPoints2(k);
				float z = kinematics[i].diagram.bodies[j]->polygons[k].depth1;
				float depth = kinematics[i].diagram.bodies[j]->polygons[k].depth2 - kinematics[i].diagram.bodies[j]->polygons[k].depth1;
				glutils::drawPrism(pts, pts2, depth, glm::vec4(0.7, 1, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
			}

			QString name = QString("body_%1_%2").arg(i).arg(j);
			QString filename = dirname + "/" + name + ".stl";
			STLExporter::save(filename, name, vertices);
		}

		// generate geometry of links
		if (linkage_type == LINKAGE_4R) {
			for (int j = 0; j < kinematics[i].diagram.links.size(); j++) {
				// For the coupler, we can use the moving body itself as a coupler, 
				// so we do not need to create a coupler link.
				if (!kinematics[i].diagram.links[j]->actual_link) continue;

				glm::dvec2& p1 = kinematics[i].diagram.links[j]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[j]->joints[1]->pos;
				std::vector<glm::dvec2> pts = kinematics::generateRoundedBarPolygon(glm::vec2(), p2 - p1, kinematics::options->link_width / 2);
				std::vector<std::vector<glm::dvec2>> holes(2);
				holes[0] = kinematics::generateCirclePolygon(glm::vec2(), kinematics::options->hole_radius);
				holes[1] = kinematics::generateCirclePolygon(p2 - p1, kinematics::options->hole_radius);

				std::vector<Vertex> vertices;

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				//float z = kinematics::options->link_depth + kinematics::options->joint_cap_depth;
				float z = kinematics::options->link_depth + kinematics::options->joint_cap_depth + kinematics[i].diagram.links[j]->z * (kinematics::options->link_depth + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth) + kinematics::options->gap;
				glutils::drawPrism(pts, holes, kinematics::options->link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::mat4(), vertices);

				QString name = QString("link_%1_%2").arg(i).arg(j);
				QString filename = dirname + "/" + name + ".stl";
				STLExporter::save(filename, name, vertices);
			}
		}
		/*
		else if (linkage_type == LINKAGE_RRRP) {
			// link 0
			{
				glm::dvec2& p1 = kinematics[i].diagram.links[0]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[0]->joints[1]->pos;
				if (!kinematics[i].diagram.links[0]->joints[0]->ground) {
					std::swap(p1, p2);
				}
				std::vector<glm::dvec2> pts = generateRoundedBarPolygon(glm::vec2(p1.x, p1.y), glm::vec2(p2.x, p2.y), link_radius);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				float z = (link_depth + joint_depth) + slider_depth - (slider_depth - slider_bar_depth) * 0.5 + joint_depth + (link_depth + joint_depth);
				glutils::drawPrism(pts, link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - link_depth)), vertices);

				// joints
				float height1 = joint_depth + slider_depth - (slider_depth - slider_bar_depth) * 0.5 + joint_depth + (link_depth + joint_depth) * 2;
				float height2 = link_depth + joint_depth * 2;
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height1, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p1.x, p1.y, link_depth)), vertices, 36);
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height2, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p2.x, p2.y, z - joint_depth)), vertices, 36);

				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height1, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p1.x, p1.y, -10 - link_depth - height1)), vertices, 36);
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height2, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p2.x, p2.y, -10 - z + joint_depth - height2)), vertices, 36);
			}

			// link 1
			{
				glm::dvec2& p1 = kinematics[i].diagram.links[1]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[1]->joints[1]->pos;
				if (!kinematics[i].diagram.links[0]->joints[0]->ground) {
					std::swap(p1, p2);
				}
				std::vector<glm::dvec2> pts = kinematics::generateBarPolygon(glm::vec2(p1.x, p1.y), glm::vec2(p2.x, p2.y), slider_bar_with);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				float z = link_depth + joint_depth;
				glutils::drawPrism(pts, slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - slider_bar_depth)), vertices);

				// slider
				glm::vec2 vec = glm::normalize(p2 - p1);
				vec *= slider_depth * 2;
				std::vector<glm::dvec2> pts2 = kinematics::generateBarPolygon(glm::vec2(p2.x - vec.x, p2.y - vec.y), glm::vec2(p2.x + vec.x, p2.y + vec.y), slider_with);
				glutils::drawPrism(pts2, slider_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z - (slider_depth - slider_bar_depth) * 0.5)), vertices);
				glutils::drawPrism(pts2, slider_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z + (slider_depth - slider_bar_depth) * 0.5 - slider_depth)), vertices);

				// joints
				float height = slider_bar_depth + joint_depth * 2;
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p1.x, p1.y, z - joint_depth)), vertices, 36);
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p2.x, p2.y, z + (slider_depth - slider_bar_depth) * 0.5)), vertices, 36);

				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p1.x, p1.y, -10 - z + joint_depth - height)), vertices, 36);
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p2.x, p2.y, -10 - z - (slider_depth - slider_bar_depth) * 0.5 - height)), vertices, 36);
			}

			// link 2
			{
				glm::dvec2& p1 = kinematics[i].diagram.links[2]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[2]->joints[1]->pos;
				std::vector<glm::dvec2> pts = generateRoundedBarPolygon(glm::vec2(p1.x, p1.y), glm::vec2(p2.x, p2.y), link_radius);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				float z = (link_depth + joint_depth) + slider_depth - (slider_depth - slider_bar_depth) * 0.5 + joint_depth;
				glutils::drawPrism(pts, link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - link_depth)), vertices);

				// joints
				float height = link_depth + joint_depth * 2;
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p1.x, p1.y, z - joint_depth)), vertices, 36);
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p2.x, p2.y, z - joint_depth)), vertices, 36);

				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p1.x, p1.y, -10 - z + joint_depth - height)), vertices, 36);
				glutils::drawCylinderZ(joint_radius, joint_radius, joint_radius, joint_radius, height, glm::vec4(0.9, 0.9, 0.9, 1), glm::translate(glm::mat4(), glm::vec3(p2.x, p2.y, -10 - z + joint_depth - height)), vertices, 36);
			}
		}
		*/
	}
}

void GLWidget3D::saveSCAD(const QString& dirname) {
	for (int i = 0; i < kinematics.size(); i++) {
		// generate geometry of rigid bodies
		for (int j = 0; j < kinematics[i].diagram.bodies.size(); j++) {
			QString name = QString("body_%1_%2").arg(i).arg(j);
			QString filename = dirname + "/" + name + ".scad";
			SCADExporter::save(filename, name, kinematics[i].diagram.bodies[j]);
		}

		// generate geometry of links
		if (linkage_type == LINKAGE_4R) {
			for (int j = 0; j < kinematics[i].diagram.links.size(); j++) {
				// For the coupler, we can use the moving body itself as a coupler, 
				// so we do not need to create a coupler link.
				if (!kinematics[i].diagram.links[j]->actual_link) continue;

				glm::dvec2& p1 = kinematics[i].diagram.links[j]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[j]->joints[1]->pos;
				std::vector<glm::dvec2> pts = kinematics::generateRoundedBarPolygon(glm::vec2(), p2 - p1, kinematics::options->link_width / 2);
				std::vector<std::vector<glm::dvec2>> holes(2);
				holes[0] = kinematics::generateCirclePolygon(glm::vec2(), kinematics::options->hole_radius);
				holes[1] = kinematics::generateCirclePolygon(p2 - p1, kinematics::options->hole_radius);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				//float z = kinematics::options->link_depth + kinematics::options->joint_cap_depth;
				float z = kinematics::options->link_depth + kinematics::options->joint_cap_depth + kinematics[i].diagram.links[j]->z * (kinematics::options->link_depth + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth) + kinematics::options->gap;

				QString name = QString("link_%1_%2").arg(i).arg(j);
				QString filename = dirname + "/" + name + ".scad";
				SCADExporter::save(filename, name, pts, holes, z, kinematics::options->link_depth);
			}
		}
	}
}

glm::dvec2 GLWidget3D::screenToWorldCoordinates(const glm::dvec2& p) {
	return screenToWorldCoordinates(p.x, p.y);
}

glm::dvec2 GLWidget3D::screenToWorldCoordinates(double x, double y) {
	glm::vec2 offset = glm::vec2(camera.pos.x, -camera.pos.y) * (float)scale();
	return glm::dvec2((x - width() * 0.5 + offset.x) / scale(), -(y - height() * 0.5 + offset.y) / scale());
}

glm::dvec2 GLWidget3D::worldToScreenCoordinates(const glm::dvec2& p) {
	return glm::dvec2(width() * 0.5 + (p.x - camera.pos.x) * scale(), height() * 0.5 - (p.y - camera.pos.y) * scale());
}

double GLWidget3D::scale() {
	return camera.f() / camera.pos.z * height() * 0.5;
}

void GLWidget3D::update3DGeometry() {
	renderManager.removeObjects();
	for (int i = 0; i < layers[layer_id].shapes.size(); i++) {
		if (layers[layer_id].shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
			QString obj_name = QString("object_%1").arg(i);
			renderManager.addObject(obj_name, "", layers[layer_id].shapes[i]->getVertices(), true);
		}
	}

	// update shadow map
	renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
}

void GLWidget3D::update3DGeometryFromKinematics() {
	renderManager.removeObjects();
	std::vector<Vertex> vertices;
	for (int i = 0; i < kinematics.size(); i++) {
		// generate geometry of rigid bodies
 		for (int j = 0; j < kinematics[i].diagram.bodies.size(); j++) {
			for (int k = 0; k < kinematics[i].diagram.bodies[j]->size(); k++) {
				std::vector<glm::dvec2> points = kinematics[i].diagram.bodies[j]->getActualPoints(k);
				std::vector<glm::dvec2> points2 = kinematics[i].diagram.bodies[j]->getActualPoints2(k);
				float z = kinematics[i].diagram.bodies[j]->polygons[k].depth1;
				float depth = kinematics[i].diagram.bodies[j]->polygons[k].depth2 - kinematics[i].diagram.bodies[j]->polygons[k].depth1;
				glutils::drawPrism(points, points2, depth, glm::vec4(0.7, 1, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
			}
		}

		// generate geometry of links
		if (linkage_type == LINKAGE_4R) {
			for (int j = 0; j < kinematics[i].diagram.links.size(); j++) {
				// For the coupler, we can use the moving body itself as a coupler, 
				// so we do not need to create a coupler link.
				if (!kinematics[i].diagram.links[j]->actual_link) continue;

				glm::dvec2& p1 = kinematics[i].diagram.links[j]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[j]->joints[1]->pos;
				std::vector<glm::dvec2> pts = kinematics::generateRoundedBarPolygon(glm::vec2(p1.x, p1.y), glm::vec2(p2.x, p2.y), kinematics::options->link_width / 2);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				float z = kinematics::options->link_depth * 2 + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth + kinematics[i].diagram.links[j]->z * (kinematics::options->link_depth + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth) + kinematics::options->gap;
				glutils::drawPrism(pts, kinematics::options->link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, kinematics::options->link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - kinematics::options->link_depth)), vertices);
			}
		}
		else if (linkage_type == LINKAGE_RRRP) {
			// link 0
			{
				glm::dvec2& p1 = kinematics[i].diagram.links[0]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[0]->joints[1]->pos;
				if (!kinematics[i].diagram.links[0]->joints[0]->ground) {
					std::swap(p1, p2);
				}
				std::vector<glm::dvec2> pts = kinematics::generateRoundedBarPolygon(glm::vec2(p1.x, p1.y), glm::vec2(p2.x, p2.y), kinematics::options->link_width / 2);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				float z = kinematics::options->link_depth * 2 + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth + kinematics[i].diagram.links[0]->z * (kinematics::options->link_depth + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth) + kinematics::options->gap;
				glutils::drawPrism(pts, kinematics::options->link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, kinematics::options->link_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - kinematics::options->link_depth)), vertices);
			}

			// link 1
			{
				glm::dvec2& p1 = kinematics[i].diagram.links[1]->joints[0]->pos;
				glm::dvec2& p2 = kinematics[i].diagram.links[1]->joints[1]->pos;
				if (!kinematics[i].diagram.links[0]->joints[0]->ground) {
					std::swap(p1, p2);
				}
				std::vector<glm::dvec2> pts = kinematics::generateBarPolygon(glm::vec2(p1.x, p1.y), glm::vec2(p2.x, p2.y), kinematics::options->slider_bar_width);

				// HACK
				// This part is currently very unorganized.
				// For each type of mechanism, I have to hard code the depth of each link.
				// In the future, this part of code should be moved to the class of each type of mechanism.
				float z = kinematics::options->link_depth * 2 + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth + kinematics[i].diagram.links[1]->z * (kinematics::options->link_depth + kinematics::options->gap * 2 + kinematics::options->joint_cap_depth) + kinematics::options->gap;
				glutils::drawPrism(pts, kinematics::options->slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, kinematics::options->slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - kinematics::options->slider_bar_depth)), vertices);

				// end of bar
				pts = kinematics::generateCirclePolygon(p1, kinematics::options->slider_width / 2);
				glutils::drawPrism(pts, kinematics::options->slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, kinematics::options->slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - kinematics::options->slider_bar_depth)), vertices);
				pts = kinematics::generateCirclePolygon(p2, kinematics::options->slider_width / 2);
				glutils::drawPrism(pts, kinematics::options->slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z)), vertices);
				glutils::drawPrism(pts, kinematics::options->slider_bar_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z - kinematics::options->slider_bar_depth)), vertices);
				
				// slider
				glm::vec2 vec = glm::normalize(p2 - p1);
				vec *= kinematics::options->slider_depth * 2;
				std::vector<glm::dvec2> pts2 = kinematics::generateBarPolygon(glm::vec2(p2.x - vec.x, p2.y - vec.y), glm::vec2(p2.x + vec.x, p2.y + vec.y), kinematics::options->slider_width);
				glutils::drawPrism(pts2, kinematics::options->slider_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, z - (kinematics::options->slider_depth - kinematics::options->slider_bar_depth) * 0.5)), vertices);
				glutils::drawPrism(pts2, kinematics::options->slider_depth, glm::vec4(0.7, 0.7, 0.7, 1), glm::translate(glm::mat4(), glm::vec3(0, 0, -10 - z + (kinematics::options->slider_depth - kinematics::options->slider_bar_depth) * 0.5 - kinematics::options->slider_depth)), vertices);
			}
		}
	}
	renderManager.addObject("kinematics", "", vertices, true);

	// update shadow map
	renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
}

void GLWidget3D::calculateSolutions(int linkage_type, int num_samples, std::vector<std::pair<double, double>>& sigmas, bool avoid_branch_defect, bool rotatable_crank, double position_error_weight, double orientation_error_weight, double linkage_location_weight, double trajectory_weight, double size_weight) {
	mainWin->ui.statusBar->showMessage("Please wait for a moment...");

	// change the mode to kinematics
	setMode(MODE_KINEMATICS);
	mainWin->ui.actionKinematics->setChecked(true);

	time_t start = clock();

	this->linkage_type = linkage_type;

	// get the geometry of fixed rigid bodies, moving bodies, linkage regions
	fixed_body_pts.clear();
	body_pts.clear();
	linkage_region_pts.clear();
	poses.clear();
	for (int i = 0; i < layers[0].shapes.size(); i++) {
		int subtype = layers[0].shapes[i]->getSubType();
		if (subtype == canvas::Shape::TYPE_BODY) {
			glm::dmat3x3 mat0 = layers[0].shapes[i]->getModelMatrix();

			bool moved = false;
			for (int j = 0; j < layers.size(); j++) {
				glm::dmat3x3 mat = layers[j].shapes[i]->getModelMatrix();
				if (mat != mat0) {
					moved = true;
					break;
				}
			}

			if (moved) {
				body_pts.push_back(kinematics::Object25D(layers[0].shapes[i]->getPoints(), -10, 0));

				// calcualte poses of the moving body
				poses.resize(poses.size() + 1);
				for (int j = 0; j < layers.size(); j++) {
					poses.back().push_back(layers[j].shapes[i]->getModelMatrix());
				}
			}
			else {
				fixed_body_pts.push_back(kinematics::Object25D(layers[0].shapes[i]->getPoints(), -10, 0));
			}
		}
		else if (subtype == canvas::Shape::TYPE_LINKAGE_REGION) {
			linkage_region_pts.push_back(layers[0].shapes[i]->getPoints());
		}
	}

	// if the linkage region is not specified, use a large enough region as default
	if (linkage_region_pts.size() < poses.size()) {
		int num_linkage_regions = linkage_region_pts.size();
		linkage_region_pts.resize(poses.size());
		for (int i = num_linkage_regions; i < poses.size(); i++) {
			linkage_region_pts[i].push_back(glm::dvec2(-40, -40));
			linkage_region_pts[i].push_back(glm::dvec2(-40, 40));
			linkage_region_pts[i].push_back(glm::dvec2(40, 40));
			linkage_region_pts[i].push_back(glm::dvec2(40, -40));
		}
	}

	solutions.resize(body_pts.size(), std::vector<kinematics::Solution>(2));
	selected_solutions.resize(body_pts.size());
	for (int i = 0; i < body_pts.size(); i++) {
		time_t start = clock();

		boost::shared_ptr<kinematics::LinkageSynthesis> synthesis;
		if (linkage_type == LINKAGE_4R) {
			synthesis = boost::shared_ptr<kinematics::LinkageSynthesis>(new kinematics::LinkageSynthesis4R());
		}
		else if (linkage_type == LINKAGE_RRRP) {
			synthesis = boost::shared_ptr<kinematics::LinkageSynthesis>(new kinematics::LinkageSynthesisRRRP());
		}

		// calculate the circle point curve and center point curve
		synthesis->calculateSolution(poses[i], linkage_region_pts[i], num_samples, fixed_body_pts, body_pts[i], sigmas, rotatable_crank, avoid_branch_defect, 1.0, solutions[i]);

		if (solutions[i].size() == 0) {
			mainWin->ui.statusBar->showMessage("No candidate was found.");
		}
		else if (solutions[i].size() == 0) {
			mainWin->ui.statusBar->showMessage("1 candidate was found.");
		}
		else {
			mainWin->ui.statusBar->showMessage(QString("%1 candidates were found.").arg(solutions[i].size()));
		}

		time_t end = clock();
		std::cout << "Elapsed: " << (double)(end - start) / CLOCKS_PER_SEC << " sec for obtaining " << solutions[i].size() << " candidates." << std::endl;

		start = clock();
		if (linkage_type == LINKAGE_4R) {
			selected_solutions[i] = synthesis->findBestSolution(poses[i], solutions[i], fixed_body_pts, body_pts[i], position_error_weight, orientation_error_weight, linkage_location_weight, trajectory_weight, size_weight);
		}
		else if (linkage_type == LINKAGE_RRRP) {
			selected_solutions[i] = synthesis->findBestSolution(poses[i], solutions[i], fixed_body_pts, body_pts[i], position_error_weight, orientation_error_weight, linkage_location_weight, trajectory_weight, size_weight);
		}

		end = clock();
		std::cout << "Elapsed: " << (double)(end - start) / CLOCKS_PER_SEC << " sec for finding the best solution. " << std::endl;
	}

	constructKinematics();

	time_t end = clock();
	std::cout << "Total computation time was " << (double)(end - start) / CLOCKS_PER_SEC << " sec." << std::endl;
	
	// update 3D geometry from kinematics
	update3DGeometryFromKinematics();

	update();
}

/**
 * Construct a kinematic diagram based on the selected solution.
 */
void GLWidget3D::constructKinematics() {
	kinematics.clear();

	// get the geometry of fixed rigid bodies, moving bodies
	fixed_body_pts.clear();
	body_pts.clear();
	for (int i = 0; i < layers[0].shapes.size(); i++) {
		int subtype = layers[0].shapes[i]->getSubType();
		if (subtype == canvas::Shape::TYPE_BODY) {
			glm::dmat3x3 mat0 = layers[0].shapes[i]->getModelMatrix();

			bool moved = false;
			for (int j = 0; j < layers.size(); j++) {
				glm::dmat3x3 mat = layers[j].shapes[i]->getModelMatrix();
				if (mat != mat0) {
					moved = true;
					break;
				}
			}

			if (moved) {
				body_pts.push_back(kinematics::Object25D(layers[0].shapes[i]->getPoints(), -10, 0));
			}
			else {
				fixed_body_pts.push_back(kinematics::Object25D(layers[0].shapes[i]->getPoints(), -10, 0));
			}
		}
		else if (subtype == canvas::Shape::TYPE_LINKAGE_REGION) {
			linkage_region_pts.push_back(layers[0].shapes[i]->getPoints());
		}
	}

	// construct kinamtics
	for (int i = 0; i < selected_solutions.size(); i++) {
		if (linkage_type == LINKAGE_4R) {
			// construct a linkage
			kinematics::Kinematics kin;
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(0, true, selected_solutions[i].fixed_point[0], 0)));
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(1, true, selected_solutions[i].fixed_point[1], 1)));
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(2, false, selected_solutions[i].moving_point[0], 0)));
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(3, false, selected_solutions[i].moving_point[1], 1)));
			kin.diagram.addLink(true, kin.diagram.joints[0], kin.diagram.joints[2], true, 0);
			kin.diagram.addLink(false, kin.diagram.joints[1], kin.diagram.joints[3], true, 1);
			kin.diagram.addLink(false, kin.diagram.joints[2], kin.diagram.joints[3], false);

			// update the geometry
			kin.diagram.bodies.clear();
			kin.diagram.addBody(kin.diagram.joints[2], kin.diagram.joints[3], body_pts[i]);

			kinematics.push_back(kin);

			//updateDefectFlag(solution.poses, kinematics[0]);
		}
		else if (linkage_type == LINKAGE_RRRP) {
			// construct a linkage
			kinematics::Kinematics kin;
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(0, true, selected_solutions[i].fixed_point[0], 0)));
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(1, true, selected_solutions[i].fixed_point[1], 1)));
			kin.diagram.addJoint(boost::shared_ptr<kinematics::PinJoint>(new kinematics::PinJoint(2, false, selected_solutions[i].moving_point[0], 0)));
			kin.diagram.addJoint(boost::shared_ptr<kinematics::SliderHinge>(new kinematics::SliderHinge(3, false, selected_solutions[i].moving_point[1], 1)));
			kin.diagram.addLink(true, kin.diagram.joints[0], kin.diagram.joints[2], true, 0);
			kin.diagram.addLink(false, kin.diagram.joints[1], kin.diagram.joints[3], true, 1);
			kin.diagram.addLink(false, kin.diagram.joints[2], kin.diagram.joints[3], false);

			// update the geometry
			kin.diagram.bodies.clear();
			kin.diagram.addBody(kin.diagram.joints[2], kin.diagram.joints[3], body_pts[i]);

			kinematics.push_back(kin);

			//updateDefectFlag(solution.poses, kinematics[0]);
		}
	}
	
	// connect joints to rigid bodies
	for (int i = 0; i < kinematics.size(); i++) {
		kinematics[i].diagram.connectJointsToBodies(fixed_body_pts);
	}

	// add the fixed rigid bodies to the fixed joints of all the linkages
	for (int i = 0; i < fixed_body_pts.size(); i++) {
		for (int j = 0; j < kinematics.size(); j++) {
			kinematics[j].diagram.addBody(kinematics[j].diagram.joints[0], kinematics[j].diagram.joints[1], fixed_body_pts[i]);
		}
	}

	// setup the kinematic system
	for (int i = 0; i < kinematics.size(); i++) {
		kinematics[i].diagram.initialize();
	}
}

/**
* Find the closest solution.
*
* @param solutions	solution set
* @param pt		mouse position
* @param joint_id	0 -- driving crank / 1 -- follower
*/
int GLWidget3D::findSolution(const std::vector<kinematics::Solution>& solutions, const glm::dvec2& pt, int joint_id) {
	int ans = -1;
	double min_dist = std::numeric_limits<double>::max();

	for (int i = 0; i < solutions.size(); i++) {
		double dist = glm::length(solutions[i].fixed_point[joint_id] - pt);
		if (dist < min_dist) {
			min_dist = dist;
			ans = i;
		}
	}

	return ans;
}

void GLWidget3D::run() {
	if (animation_timer == NULL) {
		animation_timer = new QTimer(this);
		connect(animation_timer, SIGNAL(timeout()), this, SLOT(animation_update()));
		animation_timer->start(10);
	}
}

void GLWidget3D::stop() {
	if (animation_timer != NULL) {
		animation_timer->stop();
		delete animation_timer;
		animation_timer = NULL;
	}
}

void GLWidget3D::speedUp() {
	for (int i = 0; i < kinematics.size(); i++) {
		kinematics[i].speedUp();
	}
}

void GLWidget3D::speedDown() {
	for (int i = 0; i < kinematics.size(); i++) {
		kinematics[i].speedDown();
	}
}

void GLWidget3D::invertSpeed() {
	for (int i = 0; i < kinematics.size(); i++) {
		kinematics[i].invertSpeed();
	}
}

void GLWidget3D::stepForward() {
	if (animation_timer == NULL) {
		for (int i = 0; i < kinematics.size(); i++) {
			try {
				kinematics[i].stepForward(collision_check);
			}
			catch (char* ex) {
				kinematics[i].invertSpeed();
				std::cerr << "Animation is stopped by error:" << std::endl;
				std::cerr << ex << std::endl;
			}
		}

		update3DGeometryFromKinematics();

		update();
	}
}

void GLWidget3D::stepBackward() {
	if (animation_timer == NULL) {
		for (int i = 0; i < kinematics.size(); i++) {
			try {
				kinematics[i].stepBackward(collision_check);
			}
			catch (char* ex) {
				kinematics[i].invertSpeed();
				std::cerr << "Animation is stopped by error:" << std::endl;
				std::cerr << ex << std::endl;
			}
		}

		update3DGeometryFromKinematics();

		update();
	}
}

void GLWidget3D::keyPressEvent(QKeyEvent *e) {
	ctrlPressed = false;
	shiftPressed = false;

	if (e->modifiers() & Qt::ControlModifier) {
		ctrlPressed = true;
	}
	if (e->modifiers() & Qt::ShiftModifier) {
		shiftPressed = true;
	}

	switch (e->key()) {
	case Qt::Key_Space:
		// start/stop the animation
		if (animation_timer == NULL) {
			run();
		}
		else {
			stop();
		}
		break;
	default:
		break;
	}
}

void GLWidget3D::keyReleaseEvent(QKeyEvent* e) {
	switch (e->key()) {
	case Qt::Key_Control:
		ctrlPressed = false;
		break;
	case Qt::Key_Shift:
		shiftPressed = false;
		break;
	default:
		break;
	}
}

void GLWidget3D::animation_update() {
	for (int i = 0; i < kinematics.size(); i++) {
		try {
			kinematics[i].stepForward(collision_check);
		}
		catch (char* ex) {
			kinematics[i].invertSpeed();
			//stop();
			std::cerr << "Animation is stopped by error:" << std::endl;
			std::cerr << ex << std::endl;
		}
	}

	update3DGeometryFromKinematics();

	update();

}

/**
* This function is called once before the first call to paintGL() or resizeGL().
*/
void GLWidget3D::initializeGL() {
	// init glew
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		std::cout << "Error: " << glewGetErrorString(err) << std::endl;
	}

	if (glewIsSupported("GL_VERSION_4_2"))
		printf("Ready for OpenGL 4.2\n");
	else {
		printf("OpenGL 4.2 not supported\n");
		exit(1);
	}
	const GLubyte* text = glGetString(GL_VERSION);
	printf("VERSION: %s\n", text);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_TEXTURE_2D);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glDisable(GL_TEXTURE_2D);

	glEnable(GL_TEXTURE_3D);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glDisable(GL_TEXTURE_3D);

	glEnable(GL_TEXTURE_2D_ARRAY);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glDisable(GL_TEXTURE_2D_ARRAY);

	////////////////////////////////
	renderManager.init("", "", "", true, 8192);
	renderManager.resize(this->width(), this->height());

	glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex0"), 0);//tex0: 0
}

/**
* This function is called whenever the widget has been resized.
*/
void GLWidget3D::resizeGL(int width, int height) {
	height = height ? height : 1;
	glViewport(0, 0, width, height);
	camera.updatePMatrix(width, height);

	renderManager.resize(width, height);
}

/**
* This function is called whenever the widget needs to be painted.
*/
void GLWidget3D::paintEvent(QPaintEvent *event) {
	if (first_paint) {
		std::vector<Vertex> vertices;
		glutils::drawQuad(0.001, 0.001, glm::vec4(1, 1, 1, 1), glm::mat4(), vertices);
		renderManager.addObject("dummy", "", vertices, true);
		renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
		first_paint = false;
	}

	// draw by OpenGL
	makeCurrent();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	render();

	// unbind texture
	glActiveTexture(GL_TEXTURE0);

	// restore the settings for OpenGL
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	QPainter painter(this);
	painter.setOpacity(1.0f);
	if (abs(camera.xrot) < 10 && abs(camera.yrot) < 10) {
		// draw grid
		painter.save();
		painter.setPen(QPen(QColor(224, 224, 224), 1));
		for (int i = -200; i <= 200; i++) {
			glm::dvec2 p = worldToScreenCoordinates(glm::dvec2(i * 5, i * 5));
			painter.drawLine(p.x, 0, p.x, height());
			glm::dvec2 p2 = worldToScreenCoordinates(glm::dvec2(0, i * 5));
			painter.drawLine(0, p.y, width(), p.y);
		}
		painter.restore();

		// draw 2D
		glm::vec2 offset = glm::vec2(camera.pos.x, -camera.pos.y) * (float)scale();
		if (mode != MODE_KINEMATICS) {
			// render unselected layers as background
			for (int l = 0; l < layers.size(); ++l) {
				if (l == layer_id) continue;
				for (int i = 0; i < layers[l].shapes.size(); ++i) {
					if (layers[l].shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
						layers[l].shapes[i]->draw(painter, QPointF(width() * 0.5 - offset.x, height() * 0.5 - offset.y), scale());
					}
				}
			}

			// make the unselected layers faded
			painter.setPen(QColor(255, 255, 255, 160));
			painter.setBrush(QColor(255, 255, 255, 160));
			painter.drawRect(0, 0, width(), height());

			// render selected layer
			for (int i = 0; i < layers[layer_id].shapes.size(); i++) {
				layers[layer_id].shapes[i]->draw(painter, QPointF(width() * 0.5 - offset.x, height() * 0.5 - offset.y), scale());
			}

			// render currently drawing shape
			if (current_shape) {
				current_shape->draw(painter, QPointF(width() * 0.5 - offset.x, height() * 0.5 - offset.y), scale());
			}
		}
		else {
			// make the 3D faded
			painter.setPen(QColor(255, 255, 255, 160));
			painter.setBrush(QColor(255, 255, 255, 160));
			painter.drawRect(0, 0, width(), height());

			if (show_solutions && selectedJoint.first >= 0) {
				int linkage_id = selectedJoint.first;
				painter.setPen(QPen(QColor(255, 128, 128, 64), 1));
				painter.setBrush(QBrush(QColor(255, 128, 128, 64)));
				for (int i = 0; i < solutions[linkage_id].size(); i++) {
					painter.drawEllipse(width() * 0.5 - offset.x + solutions[linkage_id][i].fixed_point[0].x * scale(), height() * 0.5 - offset.y - solutions[linkage_id][i].fixed_point[0].y * scale(), 3, 3);
				}
				painter.setPen(QPen(QColor(128, 128, 255, 64), 1));
				painter.setBrush(QBrush(QColor(128, 128, 255, 64)));
				for (int i = 0; i < solutions[linkage_id].size(); i++) {
					painter.drawEllipse(width() * 0.5 - offset.x + solutions[linkage_id][i].fixed_point[1].x * scale(), height() * 0.5 - offset.y - solutions[linkage_id][i].fixed_point[1].y * scale(), 3, 3);
				}
			}

			// draw 2D mechanism
			for (int i = 0; i < kinematics.size(); i++) {
				kinematics[i].draw(painter, QPointF(width() * 0.5 - offset.x, height() * 0.5 - offset.y), scale());
			}
		}
	}
	painter.end();

	glEnable(GL_DEPTH_TEST);
}

/**
* This event handler is called when the mouse press events occur.
*/
void GLWidget3D::mousePressEvent(QMouseEvent *e) {
	// This is necessary to get key event occured even after the user selects a menu.
	setFocus();

	if (e->buttons() & Qt::LeftButton) {
		if (mode == MODE_SELECT) {
			// hit test for rotation marker
			for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
				if (glm::length(layers[layer_id].shapes[i]->getRotationMarkerPosition(scale()) - layers[layer_id].shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start rotating
					mode = MODE_ROTATION;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::RotateOperation(screenToWorldCoordinates(e->x(), e->y()), layers[layer_id].shapes[i]->worldCoordinate(layers[layer_id].shapes[i]->getCenter())));
					selected_shape = layers[layer_id].shapes[i];
					if (!layers[layer_id].shapes[i]->isSelected()) {
						unselectAll();
						layers[layer_id].shapes[i]->select();
					}
					update();
					return;
				}
			}

			// hit test for resize marker
			for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
				canvas::BoundingBox bbox = layers[layer_id].shapes[i]->boundingBox();
				if (glm::length(bbox.minPt - layers[layer_id].shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layers[layer_id].shapes[i]->worldCoordinate(bbox.maxPt)));
					selected_shape = layers[layer_id].shapes[i];
					if (!layers[layer_id].shapes[i]->isSelected()) {
						unselectAll();
						layers[layer_id].shapes[i]->select();
					}
					update();
					return;
				}

				if (glm::length(glm::dvec2(bbox.maxPt.x, bbox.minPt.y) - layers[layer_id].shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layers[layer_id].shapes[i]->worldCoordinate(glm::dvec2(bbox.minPt.x, bbox.maxPt.y))));
					selected_shape = layers[layer_id].shapes[i];
					if (!layers[layer_id].shapes[i]->isSelected()) {
						unselectAll();
						layers[layer_id].shapes[i]->select();
					}
					update();
					return;
				}

				if (glm::length(glm::dvec2(bbox.minPt.x, bbox.maxPt.y) - layers[layer_id].shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layers[layer_id].shapes[i]->worldCoordinate(glm::dvec2(bbox.maxPt.x, bbox.minPt.y))));
					selected_shape = layers[layer_id].shapes[i];
					if (!layers[layer_id].shapes[i]->isSelected()) {
						unselectAll();
						layers[layer_id].shapes[i]->select();
					}
					update();
					return;
				}

				if (glm::length(bbox.maxPt - layers[layer_id].shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layers[layer_id].shapes[i]->worldCoordinate(bbox.minPt)));
					selected_shape = layers[layer_id].shapes[i];
					if (!layers[layer_id].shapes[i]->isSelected()) {
						unselectAll();
						layers[layer_id].shapes[i]->select();
					}
					update();
					return;
				}
			}

			// hit test for the selected shapes first
			for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
				if (layers[layer_id].shapes[i]->isSelected()) {
					if (layers[layer_id].shapes[i]->hit(screenToWorldCoordinates(e->x(), e->y()))) {
						// reselecting the already selected shapes
						mode = MODE_MOVE;
						operation = boost::shared_ptr<canvas::Operation>(new canvas::MoveOperation(screenToWorldCoordinates(e->x(), e->y())));
						update();
						return;
					}
				}
			}

			// hit test for the shape
			for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
				if (layers[layer_id].shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
					if (layers[layer_id].shapes[i]->hit(screenToWorldCoordinates(e->x(), e->y()))) {
						// start moving
						mode = MODE_MOVE;
						operation = boost::shared_ptr<canvas::Operation>(new canvas::MoveOperation(screenToWorldCoordinates(e->x(), e->y())));
						if (!layers[layer_id].shapes[i]->isSelected()) {
							if (!ctrlPressed) {
								// If CTRL is not pressed, then deselect all other shapes.
								unselectAll();
							}
							layers[layer_id].shapes[i]->select();
						}
						update();
						return;
					}
				}
			}

			// hit test for the linkage region
			for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
				if (layers[layer_id].shapes[i]->getSubType() == canvas::Shape::TYPE_LINKAGE_REGION) {
					if (layers[layer_id].shapes[i]->hit(screenToWorldCoordinates(e->x(), e->y()))) {
						// start moving
						mode = MODE_MOVE;
						operation = boost::shared_ptr<canvas::Operation>(new canvas::MoveOperation(screenToWorldCoordinates(e->x(), e->y())));
						if (!layers[layer_id].shapes[i]->isSelected()) {
							if (!ctrlPressed) {
								// If CTRL is not pressed, then deselect all other shapes.
								unselectAll();
							}
							layers[layer_id].shapes[i]->select();
						}
						update();
						return;
					}
				}
			}

			unselectAll();
		}
		else if (mode == MODE_RECTANGLE) {
			if (!current_shape) {
				// start drawing a rectangle
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Rectangle(canvas::Shape::TYPE_BODY, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
		else if (mode == MODE_CIRCLE) {
			if (!current_shape) {
				// start drawing a rectangle
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Circle(canvas::Shape::TYPE_BODY, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
		else if (mode == MODE_POLYGON) {
			if (current_shape) {
				current_shape->addPoint(current_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())));
			}
			else {
				// start drawing a rectangle
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Polygon(canvas::Shape::TYPE_BODY, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
		else if (mode == MODE_LINKAGE_REGION) {
			if (current_shape) {
				current_shape->addPoint(current_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())));
			}
			else {
				// start drawing a polygon
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Polygon(canvas::Shape::TYPE_LINKAGE_REGION, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
		else if (mode == MODE_KINEMATICS) {
			// convert the mouse position to the world coordinate system
			glm::dvec2 pt = screenToWorldCoordinates(e->x(), e->y());
			//glm::dvec2 pt((e->x() - origin.x()) / scale, -(e->y() - origin.y()) / scale);

			// select a joint to move
			selectedJoint = std::make_pair(-1, -1);
			double min_dist = 6;
			for (int i = 0; i < kinematics.size(); i++) {
				for (int j = 0; j < kinematics[i].diagram.joints.size(); j++) {
					if (!ctrlPressed && j >= 2) continue;
					double dist = glm::length(kinematics[i].diagram.joints[j]->pos - pt);
					if (dist < min_dist) {
						min_dist = dist;
						selectedJoint = std::make_pair(i, j);
					}
				}
			}
		}
	}
	else if (e->buttons() & Qt::RightButton) {
		camera.mousePress(e->x(), e->y());
	}
}

/**
* This event handler is called when the mouse move events occur.
*/

void GLWidget3D::mouseMoveEvent(QMouseEvent *e) {
	if (e->buttons() & Qt::RightButton) {
		if (shiftPressed) {
			camera.move(e->x(), e->y());
		}
		else {
			camera.rotate(e->x(), e->y(), (ctrlPressed ? 0.1 : 1));
		}
	}
	else if (mode == MODE_MOVE) {
		boost::shared_ptr<canvas::MoveOperation> op = boost::static_pointer_cast<canvas::MoveOperation>(operation);
		glm::dvec2 dir = screenToWorldCoordinates(e->x(), e->y()) - op->pivot;
		for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
			if (layers[layer_id].shapes[i]->isSelected()) {
				layers[layer_id].shapes[i]->translate(dir);

				if (layers[layer_id].shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
					// update 3D geometry
					QString obj_name = QString("object_%1").arg(i);
					renderManager.removeObject(obj_name);
					renderManager.addObject(obj_name, "", layers[layer_id].shapes[i]->getVertices(), true);

					// update shadow map
					renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
				}
			}
		}
		op->pivot = screenToWorldCoordinates(e->x(), e->y());
		update();
	}
	else if (mode == MODE_ROTATION) {
		boost::shared_ptr<canvas::RotateOperation> op = boost::static_pointer_cast<canvas::RotateOperation>(operation);
		glm::dvec2 dir1 = op->pivot - op->rotation_center;
		glm::dvec2 dir2 = screenToWorldCoordinates(e->x(), e->y()) - op->rotation_center;
		double theta = atan2(dir2.y, dir2.x) - atan2(dir1.y, dir1.x);
		for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
			if (layers[layer_id].shapes[i]->isSelected()) {
				layers[layer_id].shapes[i]->rotate(theta);

				if (layers[layer_id].shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
					// update 3D geometry
					QString obj_name = QString("object_%1").arg(i);
					renderManager.removeObject(obj_name);
					renderManager.addObject(obj_name, "", layers[layer_id].shapes[i]->getVertices(), true);

					// update shadow map
					renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
				}
			}
		}
		op->pivot = screenToWorldCoordinates(e->x(), e->y());
		update();
	}
	else if (mode == MODE_RESIZE) {
		boost::shared_ptr<canvas::ResizeOperation> op = boost::static_pointer_cast<canvas::ResizeOperation>(operation);
		glm::dvec2 resize_center = selected_shape->localCoordinate(op->resize_center);
		glm::dvec2 dir1 = selected_shape->localCoordinate(op->pivot) - resize_center;
		glm::dvec2 dir2 = selected_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())) - resize_center;
		glm::dvec2 resize_scale(dir2.x / dir1.x, dir2.y / dir1.y);
		for (int i = 0; i < layers[layer_id].shapes.size(); ++i) {
			if (layers[layer_id].shapes[i]->isSelected()) {
				// resize the shape for all the layers in order to make the size of the shape the same across the layers
				for (int l = 0; l < layers.size(); l++) {
					layers[l].shapes[i]->resize(resize_scale, resize_center);
				}

				if (layers[layer_id].shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
					// update 3D geometry
					QString obj_name = QString("object_%1").arg(i);
					renderManager.removeObject(obj_name);
					renderManager.addObject(obj_name, "", layers[layer_id].shapes[i]->getVertices(), true);

					// update shadow map
					renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
				}
			}
		}
		op->pivot = screenToWorldCoordinates(e->x(), e->y());
		update();
	}
	else if (mode == MODE_RECTANGLE || mode == MODE_CIRCLE || mode == MODE_POLYGON || mode == MODE_LINKAGE_REGION) {
		if (current_shape) {
			current_shape->updateByNewPoint(current_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())), shiftPressed);
		}
	}
	else if (mode == MODE_KINEMATICS) {
		if (selectedJoint.first >= 0) {
			int linkage_id = selectedJoint.first;
			int joint_id = selectedJoint.second;
			glm::dvec2 pt = screenToWorldCoordinates(e->x(), e->y());

			if (ctrlPressed) {
				// move the selected joint
				if (joint_id < 2) {
					selected_solutions[linkage_id].fixed_point[joint_id] = pt;
				}
				else {
					selected_solutions[linkage_id].moving_point[joint_id - 2] = pt;
				}
				kinematics[linkage_id].diagram.joints[joint_id]->pos = pt;

				//updateDefectFlag(poses[linkage_id], kinematics[linkage_id]);
			}
			else {
				// select a solution
				glm::dvec2 pt = screenToWorldCoordinates(e->x(), e->y());
				int selectedSolution = findSolution(solutions[linkage_id], pt, joint_id);

				if (selectedSolution >= 0) {
					selected_solutions[linkage_id] = solutions[linkage_id][selectedSolution];

					// move the selected joint (center point)
					kinematics[linkage_id].diagram.joints[joint_id]->pos = solutions[linkage_id][selectedSolution].fixed_point[joint_id];

					// move the other end joint (circle point)
					kinematics[linkage_id].diagram.joints[joint_id + 2]->pos = solutions[linkage_id][selectedSolution].moving_point[joint_id];

					// initialize the other link
					joint_id = 1 - joint_id;
					kinematics[linkage_id].diagram.joints[joint_id]->pos = solutions[linkage_id][selectedSolution].fixed_point[joint_id];
					kinematics[linkage_id].diagram.joints[joint_id + 2]->pos = solutions[linkage_id][selectedSolution].moving_point[joint_id];

					// initialize the other linkages
					for (int i = 0; i < kinematics.size(); i++) {
						if (i == linkage_id) continue;

						int selectedSolution = findSolution(solutions[i], kinematics[i].diagram.joints[0]->pos, 0);
						if (selectedSolution >= 0) {
							kinematics[i].diagram.joints[2]->pos = solutions[i][selectedSolution].moving_point[0];
							kinematics[i].diagram.joints[3]->pos = solutions[i][selectedSolution].moving_point[1];
						}
					}

					//updateDefectFlag(solutions[linkage_id][selectedSolution].poses, kinematics[linkage_id]);
				}
			}

			// update the geometry
			for (int i = 0; i < body_pts.size(); i++) {
				kinematics[i].diagram.bodies.clear();
				kinematics[i].diagram.addBody(kinematics[i].diagram.joints[2], kinematics[i].diagram.joints[3], body_pts[i]);
			}
			for (int i = 0; i < fixed_body_pts.size(); i++) {
				for (int j = 0; j < kinematics.size(); j++) {
					kinematics[j].diagram.addBody(kinematics[j].diagram.joints[0], kinematics[j].diagram.joints[1], fixed_body_pts[i]);
				}
			}

			// setup the kinematic system
			for (int i = 0; i < kinematics.size(); i++) {
				kinematics[i].diagram.initialize();
			}
			update();

			//updateDefectFlag(poses[linkage_id], kinematics[linkage_id]);
		}
	}

	update();
}

/**
* This event handler is called when the mouse release events occur.
*/
void GLWidget3D::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::RightButton) {
		//if (abs(camera.xrot) < 20 && abs(camera.yrot) < 20) {
		camera.xrot = 0;
		camera.yrot = 0;
		camera.updateMVPMatrix();
		front_faced = true;
		/*
		}
		else {
		front_faced = false;
		}
		*/
	}
	else if (mode == MODE_MOVE || mode == MODE_ROTATION || mode == MODE_RESIZE) {
		history.push(layers);
		mode = MODE_SELECT;
	}
	else if (mode == MODE_KINEMATICS) {
		constructKinematics();
		update3DGeometryFromKinematics();
	}

	update();
}

void GLWidget3D::mouseDoubleClickEvent(QMouseEvent* e) {
	if (mode == MODE_RECTANGLE || mode == MODE_CIRCLE || mode == MODE_POLYGON || mode == MODE_LINKAGE_REGION) {
		if (e->button() == Qt::LeftButton) {
			if (current_shape) {
				// The shape is created.
				current_shape->completeDrawing();
				for (int i = 0; i < layers.size(); i++) {
					layers[i].shapes.push_back(current_shape->clone());
				}

				// update 3D geometry
				update3DGeometry();

				layers[layer_id].shapes.back()->select();
				mode = MODE_SELECT;
				history.push(layers);
				current_shape.reset();
				operation.reset();
				mainWin->ui.actionSelect->setChecked(true);
			}
		}
	}

	setMouseTracking(false);

	update();
}

void GLWidget3D::wheelEvent(QWheelEvent* e) {
	camera.zoom(e->delta() * 0.1);
	update();
}
