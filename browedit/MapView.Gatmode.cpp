#define IMGUI_DEFINE_MATH_OPERATORS
#include <Windows.h>
#include "MapView.h"

#include <browedit/BrowEdit.h>
#include <browedit/Map.h>
#include <browedit/Node.h>
#include <browedit/Icons.h>
#include <browedit/gl/FBO.h>
#include <browedit/gl/Texture.h>
#include <browedit/shaders/SimpleShader.h>
#include <browedit/components/Gat.h>
#include <browedit/components/GatRenderer.h>
#include <browedit/components/Rsw.h>
#include <browedit/math/Polygon.h>
#include <browedit/actions/GroupAction.h>
#include <browedit/actions/TileSelectAction.h>
#include <browedit/actions/CubeHeightChangeAction.h>
#include <browedit/actions/CubeTileChangeAction.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <queue>

std::string gatTypes[] = {"Walkable", "Not Walkable", "??", "Walkable,Water" , "Walkable,NoWater" , "Snipable" , "Walkable?" , "" , "" , "" , "" , "" , "" , "" , "" , ""};
extern ImVec4 enabledColor;// (144 / 255.0f, 193 / 255.0f, 249 / 255.0f, 0.5f);
extern ImVec4 disabledColor;// (72 / 255.0f, 96 / 255.0f, 125 / 255.0f, 0.5f);

float TriangleHeight(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& p);
bool TriangleContainsPoint(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& p);


void MapView::postRenderGatMode(BrowEdit* browEdit)
{
	float gridSize = gridSizeTranslate;
	float gridOffset = gridOffsetTranslate;

	auto gat = map->rootNode->getComponent<Gat>();
	auto gatRenderer = map->rootNode->getComponent<GatRenderer>();

	//options for doodling
	static int doodleSize = 0;
	static float doodleHardness = 0;
	static float doodleSpeed = 0.1f;
	
	static int gatIndex = 0;

	//options for selections
	static bool splitTriangleFlip = false;
	static bool showCenterArrow = true;
	static bool showCornerArrows = true;
	static bool showEdgeArrows = true;
	static int edgeMode = 0;

	ImGui::Begin("Gat Edit");
	ImGui::PushItemWidth(-200);

	if (browEdit->gatDoodle && browEdit->heightDoodle)
		browEdit->gatDoodle = false;

	if (ImGui::TreeNodeEx("Tool", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		for (int i = 0; i < 16; i++)
		{
			ImVec2 v1(.25f * (i % 4), .25f * (i / 4));
			ImVec2 v2(v1.x + .25f, v1.y + .25f);
			ImGui::PushID(i);
			bool clicked = ImGui::ImageButton((ImTextureID)(long long)browEdit->gatTexture->id(), ImVec2(browEdit->config.toolbarButtonSize, browEdit->config.toolbarButtonSize), v1, v2, 0, gatIndex ==i ? enabledColor : disabledColor);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(gatTypes[i].c_str());

			float last_button_x2 = ImGui::GetItemRectMax().x;
			float next_button_x2 = last_button_x2 + style.ItemSpacing.x + browEdit->config.toolbarButtonSize;


			ImGui::PopID();
			if (clicked)
			{
				gatIndex = i;
				browEdit->heightDoodle = false;
				browEdit->gatDoodle = true;
			}
			if (i < 15 && next_button_x2 < window_visible_x2)
				ImGui::SameLine();
		}
		if (browEdit->toolBarToggleButton("Height", ICON_HEIGHT_DOODLE, browEdit->heightDoodle, "Height editing", ImVec4(1, 1, 1, 1)))
		{
			browEdit->heightDoodle = true;
			browEdit->gatDoodle = false;
		}
		ImGui::SameLine();
		if (browEdit->toolBarToggleButton("Rectangle", ICON_SELECT_RECTANGLE, !browEdit->heightDoodle && !browEdit->gatDoodle && browEdit->selectTool == BrowEdit::SelectTool::Rectangle, "Rectangle Select", ImVec4(1, 1, 1, 1)))
		{
			browEdit->selectTool = BrowEdit::SelectTool::Rectangle;
			browEdit->heightDoodle = false;
			browEdit->gatDoodle = false;
		}
		ImGui::SameLine();
		if (browEdit->toolBarToggleButton("Lasso", ICON_SELECT_LASSO, !browEdit->heightDoodle && !browEdit->gatDoodle && browEdit->selectTool == BrowEdit::SelectTool::Lasso, "Lasso Select", ImVec4(1, 1, 1, 1)))
		{
			browEdit->selectTool = BrowEdit::SelectTool::Lasso;
			browEdit->heightDoodle = false;
			browEdit->gatDoodle = false;
		}
		ImGui::SameLine();
		if (browEdit->toolBarToggleButton("WandTex", ICON_SELECT_WAND_TEX, !browEdit->heightDoodle && !browEdit->gatDoodle && browEdit->selectTool == BrowEdit::SelectTool::WandTex, "Magic Wand (Texture)", ImVec4(1, 1, 1, 1)))
		{
			browEdit->selectTool = BrowEdit::SelectTool::WandTex;
			browEdit->heightDoodle = false;
			browEdit->gatDoodle = false;
		}
		ImGui::SameLine();
		if (browEdit->toolBarToggleButton("WandHeight", ICON_SELECT_WAND_HEIGHT, !browEdit->heightDoodle && !browEdit->gatDoodle && browEdit->selectTool == BrowEdit::SelectTool::WandHeight, "Magic Wand (Height)", ImVec4(1, 1, 1, 1)))
		{
			browEdit->selectTool = BrowEdit::SelectTool::WandHeight;
			browEdit->heightDoodle = false;
			browEdit->gatDoodle = false;
		}
		ImGui::SameLine();
		if (browEdit->toolBarToggleButton("AllTex", ICON_SELECT_ALL_TEX, !browEdit->heightDoodle && !browEdit->gatDoodle && browEdit->selectTool == BrowEdit::SelectTool::AllTex, "Select All (Texture)", ImVec4(1, 1, 1, 1)))
		{
			browEdit->selectTool = BrowEdit::SelectTool::AllTex;
			browEdit->heightDoodle = false;
			browEdit->gatDoodle = false;
		}
		ImGui::SameLine();
		if (browEdit->toolBarToggleButton("AllHeight", ICON_SELECT_ALL_HEIGHT, !browEdit->heightDoodle && !browEdit->gatDoodle && browEdit->selectTool == BrowEdit::SelectTool::AllHeight, "Select All (Height)", ImVec4(1, 1, 1, 1)))
		{
			browEdit->selectTool = BrowEdit::SelectTool::AllHeight;
			browEdit->heightDoodle = false;
			browEdit->gatDoodle = false;
		}
		ImGui::TreePop();
	}

	if (!browEdit->heightDoodle && !browEdit->gatDoodle)
	{
		if (ImGui::TreeNodeEx("Selection Options", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
		{
			browEdit->toolBarToggleButton("TriangleSplit", splitTriangleFlip ? ICON_HEIGHT_SPLIT_1 : ICON_HEIGHT_SPLIT_2, &splitTriangleFlip, "Change the way quads are split into triangles");
			ImGui::SameLine();
			browEdit->toolBarToggleButton("CenterArrow", ICON_HEIGHT_SELECT_CENTER, &showCenterArrow, "Show Center Arrow");
			ImGui::SameLine();
			browEdit->toolBarToggleButton("CornerArrow", ICON_HEIGHT_SELECT_CORNERS, &showCornerArrows, "Show Corner Arrows");
			ImGui::SameLine();
			browEdit->toolBarToggleButton("EdgeArrow", ICON_HEIGHT_SELECT_SIDES, &showEdgeArrows, "Show Edge Arrows");


			if (ImGui::TreeNodeEx("Edge handling", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
			{
				ImGui::RadioButton("Nothing", &edgeMode, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Raise ground", &edgeMode, 1);
				ImGui::RadioButton("Snap ground", &edgeMode, 2);
				ImGui::TreePop();
			}

			//if (ImGui::Button("Grow selection", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
			//	map->growGatSelection(browEdit);
			//if (ImGui::Button("Shrink selection", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
			//	map->shrinkGatSelection(browEdit);

			ImGui::TreePop();
		}
	} 
	else if(browEdit->heightDoodle && !browEdit->gatDoodle)
	{
		if (ImGui::TreeNodeEx("Brush Options", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragInt("Brush Size", &doodleSize, 1.0f, 0, 10);
			ImGui::DragFloat("Brush Hardness", &doodleHardness, 0.1f, 0.0f, 1.0f);
			ImGui::DragFloat("Brush Speed", &doodleSpeed, 0.01f, 0, 1);

			ImGui::TreePop();
			if (ImGui::IsKeyPressed(GLFW_KEY_KP_ADD))
				doodleSize++;
			if (ImGui::IsKeyPressed(GLFW_KEY_KP_SUBTRACT))
				doodleSize = glm::max(doodleSize-1,0);
		}
	}
	else if (browEdit->gatDoodle && !browEdit->heightDoodle)
	{
		if (ImGui::TreeNodeEx("Brush Options", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragInt("Brush Size", &doodleSize, 1.0f, 0, 10);
			ImGui::TreePop();
			if (ImGui::IsKeyPressed(GLFW_KEY_KP_ADD))
				doodleSize++;
			if (ImGui::IsKeyPressed(GLFW_KEY_KP_SUBTRACT))
				doodleSize = glm::max(doodleSize - 1, 0);
		}
	}
	if (ImGui::TreeNodeEx("Actions", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
	{
		/*	ImGui::BeginGroup();
			if (ImGui::Button("Smooth selection", ImVec2(ImGui::GetContentRegionAvailWidth() - 12 - browEdit->config.toolbarButtonSize, 0)))
				gnd->smoothTiles(map, browEdit, map->gatSelection, 3);
			ImGui::SameLine();
			browEdit->toolBarToggleButton("Connect", ICON_NOICON, false, "Connects tiles around selection");
			ImGui::EndGroup();
			if (ImGui::Button("Smooth selection Horizontal", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
				gnd->smoothTiles(map, browEdit, map->gatSelection, 1);
			if (ImGui::Button("Smooth selection Vertical", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
				gnd->smoothTiles(map, browEdit, map->gatSelection, 2);
			if (ImGui::Button("Flatten selection", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
				gnd->flattenTiles(map, browEdit, map->gatSelection);
			if (ImGui::Button("Connect tiles high", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
				gnd->connectHigh(map, browEdit, map->gatSelection);
			if (ImGui::Button("Connect tiles low", ImVec2(ImGui::GetContentRegionAvailWidth(), 0)))
				gnd->connectLow(map, browEdit, map->gatSelection);

			if(ImGui::TreeNodeEx("Set Height", ImGuiTreeNodeFlags_Framed))
			{
				static float height = 0;
				ImGui::InputFloat("Height", &height);
				if (ImGui::Button("Set selection to height"))
				{
					auto action = new CubeHeightChangeAction(gnd, map->gatSelection);
					for (auto t : map->gatSelection)
						for(int i = 0; i < 4; i++)
							gnd->cubes[t.x][t.y]->heights[i] = height;

					action->setNewHeights(gnd, map->gatSelection);
					map->doAction(action, browEdit);
				}
				ImGui::TreePop();
			}*/

		static bool selectionOnly = false;
		static bool setWalkable = true;
		static bool setObjectWalkable = true;
		static bool blockUnderWater = true;
		static std::vector<int> textureMap; //TODO: this won't work well with multiple maps
		auto gnd = map->rootNode->getComponent<Gnd>();
		if (textureMap.size() != gnd->textures.size())
			textureMap.resize(gnd->textures.size(), -1);

		if (ImGui::Button("AutoGat"))
		{
			browEdit->windowData.progressWindowVisible = true;
			browEdit->windowData.progressWindowProgres = 0;
			browEdit->windowData.progressWindowText = "Calculating gat";

			std::thread t([this, gnd, gat, browEdit, gatRenderer]()
			{
				auto rsw = map->rootNode->getComponent<Rsw>();
				for (int x = 0; x < gat->width; x++)
				{
					for (int y = 0; y < gat->height; y++)
					{
						if (selectionOnly && std::find(map->gatSelection.begin(), map->gatSelection.end(), glm::ivec2(x, y)) == map->gatSelection.end())
							continue;

						browEdit->windowData.progressWindowProgres = (x / (float)gat->width) + (1.0f / gat->width) * (y / (float)gat->height);

						int raiseType = -1;
						int gatType = -1;

						for (int i = 0; i < 4; i++)
						{
							glm::vec3 pos = gat->getPos(x, y, i, 0.025f);
							pos.y = 1000;
							math::Ray ray(pos, glm::vec3(0, -1, 0));
							auto height = gnd->rayCast(ray, true, x / 2 - 2, y / 2 - 2, x / 2 + 2, y / 2 + 2);

							map->rootNode->traverse([&](Node* n) {
								auto rswModel = n->getComponent<RswModel>();
								if (rswModel)
								{
									auto collider = n->getComponent<RswModelCollider>();
									auto collisions = collider->getCollisions(ray);
									if (rswModel->gatCollision)
									{
										for (const auto& c : collisions)
											height = glm::max(height, c.y);
										if (collisions.size() > 0 && rswModel->gatStraightType > 0)
											raiseType = rswModel->gatStraightType;
									}
									if (rswModel->gatType > -1 && collisions.size() > 0)
									{
										gatType = rswModel->gatType;
									}
								}
							});

							gat->cubes[x][y]->heights[i] = -height.y;
						}

						if (setWalkable)
						{
							gat->cubes[x][y]->calcNormal();
							gat->cubes[x][y]->gatType = 0;
							float angle = glm::dot(gat->cubes[x][y]->normal, glm::vec3(0, -1, 0));
							if(angle < 0.9)
								gat->cubes[x][y]->gatType = 1;
							else
							{
								bool stepHeight = false;
								glm::ivec2 t(x, y);
								for (int i = 0; i < 4; i++)
								{
									for (int ii = 0; ii < 4; ii++)
									{
										auto& connectInfo = gat->connectInfo[i][ii];
										glm::ivec2 tt = t + glm::ivec2(connectInfo.x, connectInfo.y);
										if (!gat->inMap(tt))
											continue;
										if (gat->cubes[x][y]->heights[i] - gat->cubes[tt.x][tt.y]->heights[connectInfo.z] > 5)
											stepHeight = true;
									}
								}
								if (stepHeight)
									gat->cubes[x][y]->gatType = 1;

							}
						}
						if (blockUnderWater)
						{
							bool underWater = false;
							for (int i = 0; i < 4; i++)
								if (gat->cubes[x][y]->heights[i] > rsw->water.height)
									underWater = true;
							if (underWater)
								gat->cubes[x][y]->gatType = 1;
						}
						if (gatType > -1 && setObjectWalkable)
							gat->cubes[x][y]->gatType = gatType;
						if (raiseType > -1)
						{
							for (int i = 1; i < 4; i++)
							{
								if(raiseType == 1)
									gat->cubes[x][y]->heights[0] = glm::min(gat->cubes[x][y]->heights[0], gat->cubes[x][y]->heights[i]);
								else if(raiseType == 2)
									gat->cubes[x][y]->heights[0] = glm::max(gat->cubes[x][y]->heights[0], gat->cubes[x][y]->heights[i]);
							}
							for (int i = 1; i < 4; i++)
								gat->cubes[x][y]->heights[i] = gat->cubes[x][y]->heights[0];
						}
					}
				}
				browEdit->windowData.progressWindowProgres = 1;
				browEdit->windowData.progressWindowVisible = false;

				gatRenderer->allDirty = true;
			});
			t.detach();
		}
		ImGui::Checkbox("Gat selection only", &selectionOnly);
		ImGui::Checkbox("Set walkability", &setWalkable);
		ImGui::Checkbox("Set object walkability", &setObjectWalkable);
		ImGui::Checkbox("Make tiles under water unwalkable", &blockUnderWater);
		


		ImGui::TreePop();
	}

	/*if (map->gatSelection.size() > 0 && !browEdit->heightDoodle)
	{
		if (map->gatSelection.size() > 1)
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "WARNING, THESE SETTINGS ONLY\nWORK ON THE FIRST TILE");
		auto cube = gat->cubes[map->gatSelection[0].x][map->gatSelection[0].y];
		if (ImGui::TreeNodeEx("Tile Details", ImGuiTreeNodeFlags_Framed))
		{
			bool changed = false;
			changed |= util::DragFloat(browEdit, map, map->rootNode, "H1", &cube->h1);
			changed |= util::DragFloat(browEdit, map, map->rootNode, "H2", &cube->h2);
			changed |= util::DragFloat(browEdit, map, map->rootNode, "H3", &cube->h3);
			changed |= util::DragFloat(browEdit, map, map->rootNode, "H4", &cube->h4);


			if (changed)
				gatRenderer->setChunkDirty(map->gatSelection[0].x, map->gatSelection[0].y);
			ImGui::TreePop();
		}
	}*/
	ImGui::PopItemWidth();
	ImGui::End();

	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glm::value_ptr(nodeRenderContext.projectionMatrix));
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(nodeRenderContext.viewMatrix));
	glColor3f(1, 0, 0);
	fbo->bind();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisable(GL_TEXTURE_2D);

	simpleShader->use();
	simpleShader->setUniform(SimpleShader::Uniforms::projectionMatrix, nodeRenderContext.projectionMatrix);
	simpleShader->setUniform(SimpleShader::Uniforms::viewMatrix, nodeRenderContext.viewMatrix);
	simpleShader->setUniform(SimpleShader::Uniforms::modelMatrix, glm::mat4(1.0f));
	simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 0.0f);
	glEnable(GL_BLEND);
	glDepthMask(0);
	bool canSelect = true;


	auto mouse3D = gat->rayCast(mouseRay, viewEmptyTiles);
	glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 5), gat->height - (int)glm::floor(mouse3D.z / 5)+1);

	ImGui::Begin("Statusbar");
	ImGui::SetNextItemWidth(100.0f);
	ImGui::Text("Cursor: %d,%d", tileHovered.x, tileHovered.y);
	ImGui::SameLine();
	ImGui::End();

	bool snap = snapToGrid;
	if (ImGui::GetIO().KeyShift)
		snap = !snap;

	//draw paste
	if (browEdit->newCubes.size() > 0)
	{
		canSelect = false;
		std::vector<VertexP3T2N3> verts;
		float dist = 0.002f * cameraDistance;
		for (auto cube : browEdit->newCubes)
		{
			verts.push_back(VertexP3T2N3(glm::vec3(5 * (tileHovered.x + cube->pos.x), -cube->h3 + dist, 5 * gat->height - 5 * (tileHovered.y + cube->pos.y)), glm::vec2(0), cube->normals[2]));
			verts.push_back(VertexP3T2N3(glm::vec3(5 * (tileHovered.x + cube->pos.x) + 5, -cube->h2 + dist, 5 * gat->height - 5 * (tileHovered.y + cube->pos.y) + 5), glm::vec2(0), cube->normals[1]));
			verts.push_back(VertexP3T2N3(glm::vec3(5 * (tileHovered.x + cube->pos.x) + 5, -cube->h4 + dist, 5 * gat->height - 5 * (tileHovered.y + cube->pos.y)), glm::vec2(0), cube->normals[3]));

			verts.push_back(VertexP3T2N3(glm::vec3(5 * (tileHovered.x + cube->pos.x), -cube->h1 + dist, 5 * gat->height - 5 * (tileHovered.y + cube->pos.y) + 5), glm::vec2(0), cube->normals[0]));
			verts.push_back(VertexP3T2N3(glm::vec3(5 * (tileHovered.x + cube->pos.x), -cube->h3 + dist, 5 * gat->height - 5 * (tileHovered.y + cube->pos.y)), glm::vec2(0), cube->normals[2]));
			verts.push_back(VertexP3T2N3(glm::vec3(5 * (tileHovered.x + cube->pos.x) + 5, -cube->h2 + dist, 5 * gat->height - 5 * (tileHovered.y + cube->pos.y) + 5), glm::vec2(0), cube->normals[1]));
		}
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glDisableVertexAttribArray(3);
		glDisableVertexAttribArray(4); //TODO: vao
		glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data);
		glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 3);
		glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 5);

		simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(0, 1, 0, 0.25f));
		glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());


		if (ImGui::IsMouseReleased(0))
		{
			std::vector<glm::ivec2> cubeSelected;
			for (auto c : browEdit->newCubes)
				if(gat->inMap(c->pos + tileHovered))
					cubeSelected.push_back(c->pos + tileHovered);
			auto action1 = new CubeHeightChangeAction<Gat, Gat::Cube>(gat, cubeSelected);
			//auto action2 = new CubeTileChangeAction(gnd, cubeSelected);
			for (auto cube : browEdit->newCubes)
			{
				for (int i = 0; i < 4; i++)
					gat->cubes[tileHovered.x + cube->pos.x][tileHovered.y + cube->pos.y]->heights[i] = cube->heights[i];
				gat->cubes[tileHovered.x + cube->pos.x][tileHovered.y + cube->pos.y]->normal = cube->normal;
			}
			action1->setNewHeights(gat, cubeSelected);
			//action2->setNewTiles(gnd, cubeSelected);

			auto ga = new GroupAction();
			ga->addAction(action1);
			//ga->addAction(action2);
			ga->addAction(new GatTileSelectAction(map, cubeSelected));
			map->doAction(ga, browEdit);
			for (auto c : browEdit->newCubes)
				delete c;
			browEdit->newCubes.clear();
		}


	}

	if (browEdit->heightDoodle && hovered)
	{
		static std::map<Gat::Cube*, float[4]> originalHeights;
		glm::vec2 tileHoveredOffset((mouse3D.x / 5) - tileHovered.x, tileHovered.y - (gat->height - mouse3D.z / 5) - 1);
		int index = 0;
		if (tileHoveredOffset.x < 0.5 && tileHoveredOffset.y > 0.5)
			index = 0;
		if (tileHoveredOffset.x > 0.5 && tileHoveredOffset.y > 0.5)
			index = 1;
		if (tileHoveredOffset.x < 0.5 && tileHoveredOffset.y < 0.5)
			index = 2;
		if (tileHoveredOffset.x > 0.5 && tileHoveredOffset.y < 0.5)
			index = 3;

		
		float minMax = ImGui::GetIO().KeyShift ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();
		if (ImGui::GetIO().KeyCtrl)
		{
			minMax = ImGui::GetIO().KeyShift ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
			for (int x = 0; x <= doodleSize; x++)
			{
				for (int y = 0; y <= doodleSize; y++)
				{
					glm::ivec2 t = tileHovered + glm::ivec2(x, y) - glm::ivec2(doodleSize / 2);
					for (int ii = 0; ii < 4; ii++)
					{
						glm::ivec2 tt(t.x + gat->connectInfo[index][ii].x, t.y + gat->connectInfo[index][ii].y);
						if (!gat->inMap(tt))
							continue;
						int ti = gat->connectInfo[index][ii].z;
						if (ImGui::GetIO().KeyShift)
							minMax = glm::max(minMax, gat->cubes[tt.x][tt.y]->heights[ti]);
						else
							minMax = glm::min(minMax, gat->cubes[tt.x][tt.y]->heights[ti]);
					}
				}
			}
		}

		ImGui::Begin("Statusbar");
		ImGui::Text("Change Height, hold shift to lower, hold Ctrl to snap");
		ImGui::End();

		for (int x = 0; x <= doodleSize; x++)
		{
			for (int y = 0; y <= doodleSize; y++)
			{
				glm::ivec2 t = tileHovered + glm::ivec2(x, y) - glm::ivec2(doodleSize / 2);
				for (int ii = 0; ii < 4; ii++)
				{
					glm::ivec2 tt(t.x + gat->connectInfo[index][ii].x, t.y + gat->connectInfo[index][ii].y);
					if (!gat->inMap(tt))
						continue;
					int ti = gat->connectInfo[index][ii].z;
					float& snapHeight = gat->cubes[tt.x][tt.y]->heights[ti];
					if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						if (originalHeights.find(gat->cubes[tt.x][tt.y]) == originalHeights.end())
							for (int i = 0; i < 4; i++)
								originalHeights[gat->cubes[tt.x][tt.y]][i] = gat->cubes[tt.x][tt.y]->heights[i];

						if (ImGui::GetIO().KeyShift)
							snapHeight = glm::min(minMax, snapHeight + doodleSpeed * (glm::mix(1.0f, glm::max(0.0f, 1.0f - glm::length(glm::vec2(x - doodleSize / 2.0f, y - doodleSize / 2.0f)) / doodleSize), doodleHardness)));
						else
							snapHeight = glm::max(minMax, snapHeight - doodleSpeed * (glm::mix(1.0f, glm::max(0.0f, 1.0f - glm::length(glm::vec2(x - doodleSize / 2.0f, y - doodleSize / 2.0f)) / doodleSize), doodleHardness)));

						gat->cubes[tt.x][tt.y]->calcNormal();
						gatRenderer->setChunkDirty(tt.x, tt.y);
					}
					glUseProgram(0);
					glPointSize(10.0);
					glBegin(GL_POINTS);
					glVertex3f(tt.x * 5.0f + ((ti % 2) == 1 ? 5.0f : 0.0f),
						-snapHeight + 1,
						5+(gat->height - tt.y) * 5.0f + ((ti / 2) == 0 ? 5.0f : 0.0f));
					glEnd();
				}
			}
		}

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (originalHeights.size() > 0)
			{
				std::map<Gat::Cube*, float[4]> newHeights;
				for(auto kv : originalHeights)
					for (int i = 0; i < 4; i++)
						newHeights[kv.first][i] = kv.first->heights[i];

				map->doAction(new CubeHeightChangeAction<Gat, Gat::Cube>(originalHeights, newHeights), browEdit);
			}
			originalHeights.clear();
		}

	}
	else if (browEdit->gatDoodle && hovered)
	{
		std::vector<VertexP3T2N3> verts;
		float dist = 0.002f * cameraDistance;

		for (int x = 0; x <= doodleSize; x++)
		{
			for (int y = 0; y <= doodleSize; y++)
			{
				glm::ivec2 tile = tileHovered + glm::ivec2(x,y) - glm::ivec2(doodleSize/2, doodleSize/2);
				if (gat->inMap(tile))
				{
					auto cube = gat->cubes[tile.x][tile.y];
					glm::vec2 t1(.25f * (gatIndex % 4), .25f * (gatIndex / 4));
					glm::vec2 t2 = t1 + glm::vec2(.25f);

					verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x, -cube->h3 + dist, 5 * gat->height - 5 * tile.y + 5), glm::vec2(t1.x, t1.y), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x + 5, -cube->h2 + dist, 5 * gat->height - 5 * tile.y + 10), glm::vec2(t2.x, t2.y), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x + 5, -cube->h4 + dist, 5 * gat->height - 5 * tile.y + 5), glm::vec2(t2.x, t1.y), cube->normal));

					verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x, -cube->h1 + dist, 5 * gat->height - 5 * tile.y + 10), glm::vec2(t1.x, t2.y), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x, -cube->h3 + dist, 5 * gat->height - 5 * tile.y + 5), glm::vec2(t1.x, t1.y), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x + 5, -cube->h2 + dist, 5 * gat->height - 5 * tile.y + 10), glm::vec2(t2.x, t2.y), cube->normal));

					if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						gat->cubes[tile.x][tile.y]->gatType = gatIndex;
						gatRenderer->setChunkDirty(tile.x, tile.y);
					}
				}
			}
		}



		if(verts.size() > 0)
		{
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			glDisableVertexAttribArray(4); //TODO: vao
			glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data);
			glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 3);
			glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 5);


			browEdit->gatTexture->bind();
			simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 1.0f);
			simpleShader->setUniform(SimpleShader::Uniforms::lightMin, 1.0f);
			glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());
		}

	}
	else if(hovered)
	{
		//draw selection
		if (map->gatSelection.size() > 0 && canSelect)
		{
			std::vector<VertexP3T2N3> verts;
			float dist = 0.002f * cameraDistance;
			for (auto& tile : map->gatSelection)
			{
				auto cube = gat->cubes[tile.x][tile.y];
				verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x,		-cube->h3 + dist, 5 * gat->height - 5 * tile.y + 5), glm::vec2(0), cube->normal));
				verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x + 5,	-cube->h2 + dist, 5 * gat->height - 5 * tile.y + 10), glm::vec2(0), cube->normal));
				verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x + 5,	-cube->h4 + dist, 5 * gat->height - 5 * tile.y + 5), glm::vec2(0), cube->normal));

				verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x,		-cube->h1 + dist, 5 * gat->height - 5 * tile.y + 10), glm::vec2(0), cube->normal));
				verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x,		-cube->h3 + dist, 5 * gat->height - 5 * tile.y + 5), glm::vec2(0), cube->normal));
				verts.push_back(VertexP3T2N3(glm::vec3(5 * tile.x + 5,	-cube->h2 + dist, 5 * gat->height - 5 * tile.y + 10), glm::vec2(0), cube->normal));
			}
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			glDisableVertexAttribArray(4); //TODO: vao
			glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data);
			glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 3);
			glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 5);

			simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 0.0f);
			simpleShader->setUniform(SimpleShader::Uniforms::lightMin, 1.0f);
			glLineWidth(1.0f);
			simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1, 0, 0, 1.0f));
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1, 0, 0, 0.25f));
			glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());


			Gadget::setMatrices(nodeRenderContext.projectionMatrix, nodeRenderContext.viewMatrix);
			glDepthMask(1);

			glm::ivec2 maxValues(-99999, -99999), minValues(999999, 9999999);
			for (auto& s : map->gatSelection)
			{
				maxValues = glm::max(maxValues, s);
				minValues = glm::min(minValues, s);
			}
			static std::map<Gat::Cube*, float[4]> originalValues;
			static glm::vec3 originalCorners[4];

			glm::vec3 pos[9];
			pos[0] = gat->getPos(minValues.x, minValues.y, 0);
			pos[1] = gat->getPos(maxValues.x, minValues.y, 1);
			pos[2] = gat->getPos(minValues.x, maxValues.y, 2);
			pos[3] = gat->getPos(maxValues.x, maxValues.y, 3);
			pos[4] = (pos[0] + pos[1] + pos[2] + pos[3]) / 4.0f;
			pos[5] = (pos[0] + pos[1]) / 2.0f;
			pos[6] = (pos[2] + pos[3]) / 2.0f;
			pos[7] = (pos[0] + pos[2]) / 2.0f;
			pos[8] = (pos[1] + pos[3]) / 2.0f;

			ImGui::Begin("Gat Edit");
			if (ImGui::CollapsingHeader("Selection Details", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::DragFloat("Corner 1", &pos[0].y);
				ImGui::DragFloat("Corner 2", &pos[1].y);
				ImGui::DragFloat("Corner 3", &pos[2].y);
				ImGui::DragFloat("Corner 4", &pos[3].y);
			}
			ImGui::End();


			static int dragIndex = -1;
			for (int i = 0; i < 9; i++)
			{
				if (!showCornerArrows && i < 4)
					continue;
				if (!showCenterArrow && i == 4)
					continue;
				if (!showEdgeArrows && i > 4)
					continue;
				glm::mat4 mat = glm::translate(glm::mat4(1.0f), pos[i]);
				if (maxValues.x - minValues.x <= 1 || maxValues.y - minValues.y <= 1)
					mat = glm::scale(mat, glm::vec3(0.25f, 0.25f, 0.25f));

				if (dragIndex == -1)
					gadgetHeight[i].disabled = false;
				else
					gadgetHeight[i].disabled = dragIndex != i;

				gadgetHeight[i].draw(mouseRay, mat);

				if (gadgetHeight[i].axisClicked)
				{
					dragIndex = i;
					mouseDragPlane.normal = glm::normalize(glm::vec3(nodeRenderContext.viewMatrix * glm::vec4(0, 0, 1, 1)) - glm::vec3(nodeRenderContext.viewMatrix * glm::vec4(0, 0, 0, 1)));
					mouseDragPlane.normal.y = 0;
					mouseDragPlane.normal = glm::normalize(mouseDragPlane.normal);
					mouseDragPlane.D = -glm::dot(pos[i], mouseDragPlane.normal);

					float f;
					mouseRay.planeIntersection(mouseDragPlane, f);
					mouseDragStart = mouseRay.origin + f * mouseRay.dir;

					originalValues.clear();
					for (auto& t : map->gatSelection)
						for (int ii = 0; ii < 4; ii++)
							originalValues[gat->cubes[t.x][t.y]][ii] = gat->cubes[t.x][t.y]->heights[ii];

					for (int ii = 0; ii < 4; ii++)
						originalCorners[ii] = pos[ii];

					canSelect = false;
				}
				else if (gadgetHeight[i].axisReleased)
				{
					dragIndex = -1;
					canSelect = false;

					std::map<Gat::Cube*, float[4]> newValues;
					for (auto& t : originalValues)
						for (int ii = 0; ii < 4; ii++)
							newValues[t.first][ii] = t.first->heights[ii];
					map->doAction(new CubeHeightChangeAction<Gat, Gat::Cube>(originalValues, newValues), browEdit);
				}
				else if (gadgetHeight[i].axisDragged)
				{

					if (snap)
					{
						glm::mat4 mat(1.0f);
					
						glm::vec3 rounded = pos[i];
						if (!gridLocal)
							rounded.y = glm::floor(rounded.y / gridSize) * gridSize;

						mat = glm::translate(mat, rounded);
						mat = glm::rotate(mat, glm::radians(90.0f), glm::vec3(1, 0, 0));
						simpleShader->setUniform(SimpleShader::Uniforms::modelMatrix, mat);
						simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(0, 0, 0, 1));
						glLineWidth(2);
						gridVbo->bind();
						glEnableVertexAttribArray(0);
						glEnableVertexAttribArray(1);
						glDisableVertexAttribArray(2);
						glDisableVertexAttribArray(3);
						glDisableVertexAttribArray(4); //TODO: vao
						glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2), (void*)(0 * sizeof(float)));
						glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2), (void*)(3 * sizeof(float)));
						glDrawArrays(GL_LINES, 0, (int)gridVbo->size());
						gridVbo->unBind();
					}


					float snapHeight = 0;
					if (ImGui::GetIO().KeyCtrl)
					{
						if (tileHovered.x >= 0 && tileHovered.x < gat->width && tileHovered.y >= 0 && tileHovered.y < gat->height)
						{
							glm::vec2 tileHoveredOffset((mouse3D.x / 5) - tileHovered.x, tileHovered.y - (gat->height - mouse3D.z / 5));

							int index = 0;
							if (tileHoveredOffset.x > 0.5 && tileHoveredOffset.y > 0.5)
								index = 0;
							if (tileHoveredOffset.x < 0.5 && tileHoveredOffset.y > 0.5)
								index = 1;
							if (tileHoveredOffset.x > 0.5 && tileHoveredOffset.y < 0.5)
								index = 2;
							if (tileHoveredOffset.x < 0.5 && tileHoveredOffset.y < 0.5)
								index = 3;

							snapHeight = gat->cubes[tileHovered.x][tileHovered.y]->heights[index];
							glUseProgram(0);
							glPointSize(10.0);
							glBegin(GL_POINTS);
							glVertex3f(	tileHovered.x * 5.0f + ((index % 2) == 0 ? 5.0f : 0.0f), 
										-snapHeight+1, 
										(gat->height-tileHovered.y) * 5.0f + ((index / 2) == 0 ? 5.0f : 0.0f));
							glEnd();
						}
					}


					float f;
					mouseRay.planeIntersection(mouseDragPlane, f);
					glm::vec3 mouseOffset = (mouseRay.origin + f * mouseRay.dir) - mouseDragStart;
					if (snap && gridLocal)
						mouseOffset = glm::round(mouseOffset / (float)gridSize) * (float)gridSize;

					canSelect = false;
					static int masks[] = { 0b0001, 0b0010, 0b0100, 0b1000, 0b1111, 0b0011, 0b1100, 0b0101, 0b1010 };
					int mask = masks[i];

					for (int ii = 0; ii < 4; ii++)
					{
						pos[ii] = originalCorners[ii];
						if (((mask >> ii) & 1) != 0)
						{
							pos[ii].y -= mouseOffset.y;
							if (snap && !gridLocal)
							{
								pos[ii].y += 2*mouseOffset.y;
								pos[ii].y = glm::round(pos[ii].y / gridSize) * gridSize;
								pos[ii].y = originalCorners[ii].y + (originalCorners[ii].y - pos[ii].y);
							}
							if (ImGui::GetIO().KeyCtrl)
							{
								pos[ii].y = originalCorners[ii].y + (originalCorners[ii].y + snapHeight);
							}
						}
					}

					std::vector<glm::ivec2> tilesAround;
					auto isTileSelected = [&](int x, int y) { return std::find(map->gatSelection.begin(), map->gatSelection.end(), glm::ivec2(x, y)) != map->gatSelection.end(); };

					for (auto& t : map->gatSelection)
					{
						if (gatRenderer)
							gatRenderer->setChunkDirty(t.x, t.y);
						for (int ii = 0; ii < 4; ii++)
						{
							// 0 1
							// 2 3
							glm::vec3 p = gat->getPos(t.x, t.y, ii);
							if (!splitTriangleFlip)	// /
							{
								if (TriangleContainsPoint(pos[0], pos[1], pos[2], p))
									gat->cubes[t.x][t.y]->heights[ii] = originalValues[gat->cubes[t.x][t.y]][ii] + (TriangleHeight(pos[0], pos[1], pos[2], p) - TriangleHeight(originalCorners[0], originalCorners[1], originalCorners[2], p));
								if (TriangleContainsPoint(pos[1], pos[3], pos[2], p))
									gat->cubes[t.x][t.y]->heights[ii] = originalValues[gat->cubes[t.x][t.y]][ii] + (TriangleHeight(pos[1], pos[3], pos[2], p) - TriangleHeight(originalCorners[1], originalCorners[3], originalCorners[2], p));
							}
							else // \ 
							{
								if (TriangleContainsPoint(pos[0], pos[3], pos[2], p))
									gat->cubes[t.x][t.y]->heights[ii] = originalValues[gat->cubes[t.x][t.y]][ii] + (TriangleHeight(pos[0], pos[3], pos[2], p) - TriangleHeight(originalCorners[0], originalCorners[3], originalCorners[2], p));
								if (TriangleContainsPoint(pos[0], pos[1], pos[3], p))
									gat->cubes[t.x][t.y]->heights[ii] = originalValues[gat->cubes[t.x][t.y]][ii] + (TriangleHeight(pos[0], pos[1], pos[3], p) - TriangleHeight(originalCorners[0], originalCorners[1], originalCorners[3], p));
							}
							if (edgeMode == 1 || edgeMode == 2) //smooth raise area around these tiles
							{
								for (int xx = -1; xx <= 1; xx++)
									for (int yy = -1; yy <= 1; yy++)
										if (t.x + xx >= 0 && t.x + xx < gat->width && t.y + yy >= 0 && t.y + yy < gat->height &&
											std::find(tilesAround.begin(), tilesAround.end(), glm::ivec2(t.x + xx, t.y + yy)) == tilesAround.end() &&
											!isTileSelected(t.x+xx, t.y+yy))
											tilesAround.push_back(glm::ivec2(t.x + xx, t.y + yy));
							}
						}
						gat->cubes[t.x][t.y]->calcNormal();
					}

					auto getPointHeight = [&](int x1, int y1, int i1, int x2, int y2, int i2, int x3, int y3, int i3, float def)
					{
						if (x1 >= 0 && x1 < gat->width && y1 >= 0 && y1 < gat->height && isTileSelected(x1, y1))
							return gat->cubes[x1][y1]->heights[i1];
						if (x2 >= 0 && x2 < gat->width && y2 >= 0 && y2 < gat->height && isTileSelected(x2, y2))
							return gat->cubes[x2][y2]->heights[i2];
						if (x3 >= 0 && x3 < gat->width && y3 >= 0 && y3 < gat->height && isTileSelected(x3, y3))
							return gat->cubes[x3][y3]->heights[i3];
						return def;
					};
					auto getPointHeightDiff = [&](int x1, int y1, int i1, int x2, int y2, int i2, int x3, int y3, int i3)
					{
						if (x1 >= 0 && x1 < gat->width && y1 >= 0 && y1 < gat->height && isTileSelected(x1, y1))
							return originalValues[gat->cubes[x1][y1]][i1] - gat->cubes[x1][y1]->heights[i1];
						if (x2 >= 0 && x2 < gat->width && y2 >= 0 && y2 < gat->height && isTileSelected(x2, y2))
							return originalValues[gat->cubes[x2][y2]][i2] - gat->cubes[x2][y2]->heights[i2];
						if (x3 >= 0 && x3 < gat->width && y3 >= 0 && y3 < gat->height && isTileSelected(x3, y3))
							return originalValues[gat->cubes[x3][y3]][i3] - gat->cubes[x3][y3]->heights[i3];
						return 0.0f;
					};

					for (auto a : tilesAround)
					{
						auto cube = gat->cubes[a.x][a.y];
						if (originalValues.find(cube) == originalValues.end())
							for (int ii = 0; ii < 4; ii++)
								originalValues[cube][ii] = cube->heights[ii];
						//h1 bottomleft
						//h2 bottomright
						//h3 topleft
						//h4 topright
						if (edgeMode == 1) //raise ground
						{
							cube->heights[0] = originalValues[cube][0] - getPointHeightDiff(a.x - 1, a.y - 1, 3, a.x - 1, a.y, 1, a.x, a.y - 1, 2);
							cube->heights[1] = originalValues[cube][1] - getPointHeightDiff(a.x + 1, a.y - 1, 2, a.x + 1, a.y, 0, a.x, a.y - 1, 3);
							cube->heights[2] = originalValues[cube][2] - getPointHeightDiff(a.x - 1, a.y + 1, 1, a.x - 1, a.y, 3, a.x, a.y + 1, 0);
							cube->heights[3] = originalValues[cube][3] - getPointHeightDiff(a.x + 1, a.y + 1, 0, a.x + 1, a.y, 2, a.x, a.y + 1, 1);
						}
						else if (edgeMode == 2) //snap ground
						{
							cube->heights[0] = getPointHeight(a.x - 1, a.y - 1, 3, a.x - 1, a.y, 1, a.x, a.y - 1, 2, cube->heights[0]);
							cube->heights[1] = getPointHeight(a.x + 1, a.y - 1, 2, a.x + 1, a.y, 0, a.x, a.y - 1, 3, cube->heights[1]);
							cube->heights[2] = getPointHeight(a.x - 1, a.y + 1, 1, a.x - 1, a.y, 3, a.x, a.y + 1, 0, cube->heights[2]);
							cube->heights[3] = getPointHeight(a.x + 1, a.y + 1, 0, a.x + 1, a.y, 2, a.x, a.y + 1, 1, cube->heights[3]);
						}
						cube->calcNormal();
					}
				}
			}

			simpleShader->setUniform(SimpleShader::Uniforms::modelMatrix, glm::mat4(1.0f));
			glDepthMask(0);
		}




		if (hovered && canSelect)
		{
			if (ImGui::IsMouseDown(0))
			{
				if (!mouseDown)
					mouseDragStart = mouse3D;
				mouseDown = true;
			}

			if (ImGui::IsMouseReleased(0))
			{
				auto mouseDragEnd = mouse3D;
				mouseDown = false;

				std::vector<glm::ivec2> newSelection;
				if (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl)
					newSelection = map->gatSelection;

				if (browEdit->selectTool == BrowEdit::SelectTool::Rectangle)
				{
					int tileMinX = (int)glm::floor(glm::min(mouseDragStart.x, mouseDragEnd.x) / 5);
					int tileMaxX = (int)glm::ceil(glm::max(mouseDragStart.x, mouseDragEnd.x) / 5);

					int tileMaxY = gat->height - (int)glm::floor(glm::min(mouseDragStart.z, mouseDragEnd.z) / 5) + 2;
					int tileMinY = gat->height - (int)glm::ceil(glm::max(mouseDragStart.z, mouseDragEnd.z) / 5) + 2;

					if (tileMinX >= 0 && tileMaxX < gat->width + 1 && tileMinY >= 0 && tileMaxY < gat->height + 1)
						for (int x = tileMinX; x < tileMaxX; x++)
							for (int y = tileMinY; y < tileMaxY; y++)
								if (ImGui::GetIO().KeyCtrl)
								{
									if (std::find(newSelection.begin(), newSelection.end(), glm::ivec2(x, y)) != newSelection.end())
										newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == x && el.y == y; }));
								}
								else if (std::find(newSelection.begin(), newSelection.end(), glm::ivec2(x, y)) == newSelection.end())
									newSelection.push_back(glm::ivec2(x, y));
				}
				else if (browEdit->selectTool == BrowEdit::SelectTool::Lasso)
				{
					math::Polygon polygon;
					for (size_t i = 0; i < selectLasso.size(); i++)
						polygon.push_back(glm::vec2(selectLasso[i]));
					for (int x = 0; x < gat->width; x++)
					{
						for (int y = 0; y < gat->height; y++)
						{
							if(polygon.contains(glm::vec2(x, y)))
								if (ImGui::GetIO().KeyCtrl)
								{
									if (std::find(newSelection.begin(), newSelection.end(), glm::ivec2(x, y)) != newSelection.end())
										newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == x && el.y == y; }));
								}
								else if (std::find(newSelection.begin(), newSelection.end(), glm::ivec2(x, y)) == newSelection.end())
									newSelection.push_back(glm::ivec2(x, y));

						}
					}
					for (auto t : selectLasso)
						if (ImGui::GetIO().KeyCtrl)
						{
							if (std::find(newSelection.begin(), newSelection.end(), glm::ivec2(t.x, t.y)) != newSelection.end())
								newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == t.x && el.y == t.y; }));
						}
						else if (std::find(newSelection.begin(), newSelection.end(), glm::ivec2(t.x, t.y)) == newSelection.end())
							newSelection.push_back(glm::ivec2(t.x, t.y));
					selectLasso.clear();
				}
				else if (browEdit->selectTool == BrowEdit::SelectTool::WandTex)
				{
					glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 5), (gat->height - (int)glm::floor(mouse3D.z) / 5));

					std::vector<glm::ivec2> tilesToFill;
					std::queue<glm::ivec2> queue;
					std::map<int, bool> done;

					glm::ivec2 offsets[4] = { glm::ivec2(-1,0), glm::ivec2(1,0), glm::ivec2(0,-1), glm::ivec2(0,1) };
					queue.push(tileHovered);
					int c = 0;
					while (!queue.empty())
					{
						auto p = queue.front();
						queue.pop();
						for (const auto& o : offsets)
						{
							auto pp = p + o;
							if (!gat->inMap(pp) || !gat->inMap(p))
								continue;
							auto c = gat->cubes[pp.x][pp.y];
							if (c->gatType != gat->cubes[p.x][p.y]->gatType)
								continue;
							if (done.find(pp.x + gat->width * pp.y) == done.end())
							{
								done[pp.x + gat->width * pp.y] = true;
								queue.push(pp);
							}
						}
						if (gat->inMap(p) && std::find(tilesToFill.begin(), tilesToFill.end(), p) == tilesToFill.end())
							tilesToFill.push_back(p);
						c++;
					}
					for (const auto& t : tilesToFill)
					{
						if (ImGui::GetIO().KeyCtrl)
						{
							if (std::find(newSelection.begin(), newSelection.end(), t) != newSelection.end())
								newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == t.x && el.y == t.y; }));
						}
						else if (std::find(newSelection.begin(), newSelection.end(), t) == newSelection.end())
							newSelection.push_back(t);
					}
				}
				else if (browEdit->selectTool == BrowEdit::SelectTool::WandHeight)
				{
					glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 5), (gat->height - (int)glm::floor(mouse3D.z) / 5));

					std::vector<glm::ivec2> tilesToFill;
					std::queue<glm::ivec2> queue;
					std::map<int, bool> done;

					glm::ivec2 offsets[4] = { glm::ivec2(-1,0), glm::ivec2(1,0), glm::ivec2(0,-1), glm::ivec2(0,1) };
					queue.push(tileHovered);
					int c = 0;
					while (!queue.empty())
					{
						auto p = queue.front();
						queue.pop();
						for (const auto& o : offsets)
						{
							auto pp = p + o;
							if (!gat->inMap(pp) || !gat->inMap(p))
								continue;
							auto c = gat->cubes[pp.x][pp.y];

							if (!c->sameHeight(*gat->cubes[p.x][p.y]))
								continue;
							if (done.find(pp.x + gat->width * pp.y) == done.end())
							{
								done[pp.x + gat->width * pp.y] = true;
								queue.push(pp);
							}
						}
						if (gat->inMap(p) && std::find(tilesToFill.begin(), tilesToFill.end(), p) == tilesToFill.end())
							tilesToFill.push_back(p);
						c++;
					}
					for (const auto& t : tilesToFill)
					{
						if (ImGui::GetIO().KeyCtrl)
						{
							if (std::find(newSelection.begin(), newSelection.end(), t) != newSelection.end())
								newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == t.x && el.y == t.y; }));
						}
						else if (std::find(newSelection.begin(), newSelection.end(), t) == newSelection.end())
							newSelection.push_back(t);
					}
				}
				else if (browEdit->selectTool == BrowEdit::SelectTool::AllTex)
				{
					glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 5), (gat->height - (int)glm::floor(mouse3D.z) / 5));
					if (gat->inMap(tileHovered))
					{
						std::vector<glm::ivec2> tilesToFill;
						auto c = gat->cubes[tileHovered.x][tileHovered.y];
						for (int x = 0; x < gat->width; x++)
							for (int y = 0; y < gat->height; y++)
								if (c->gatType == gat->cubes[x][y]->gatType)
									tilesToFill.push_back(glm::ivec2(x, y));

						for (const auto& t : tilesToFill)
						{
							if (ImGui::GetIO().KeyCtrl)
							{
								if (std::find(newSelection.begin(), newSelection.end(), t) != newSelection.end())
									newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == t.x && el.y == t.y; }));
							}
							else if (std::find(newSelection.begin(), newSelection.end(), t) == newSelection.end())
								newSelection.push_back(t);
						}
					}
				}
				else if (browEdit->selectTool == BrowEdit::SelectTool::AllHeight)
				{
					glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 5), (gat->height - (int)glm::floor(mouse3D.z / 5)));
					if (gat->inMap(tileHovered))
					{
						std::vector<glm::ivec2> tilesToFill;
						auto c = gat->cubes[tileHovered.x][tileHovered.y];
						for (int x = 0; x < gat->width; x++)
							for (int y = 0; y < gat->height; y++)
								if (c->sameHeight(*gat->cubes[x][y]))
									tilesToFill.push_back(glm::ivec2(x, y));
						for (const auto& t : tilesToFill)
						{
							if (ImGui::GetIO().KeyCtrl)
							{
								if (std::find(newSelection.begin(), newSelection.end(), t) != newSelection.end())
									newSelection.erase(std::remove_if(newSelection.begin(), newSelection.end(), [&](const glm::ivec2& el) { return el.x == t.x && el.y == t.y; }));
							}
							else if (std::find(newSelection.begin(), newSelection.end(), t) == newSelection.end())
								newSelection.push_back(t);
						}
					}
				}


				if (map->gatSelection != newSelection)
				{
					map->doAction(new GatTileSelectAction(map, newSelection), browEdit);
				}
			}
		}
		if (mouseDown)
		{
			auto mouseDragEnd = mouse3D;
			std::vector<VertexP3T2N3> verts;
			float dist = 0.002f * cameraDistance;

			int tileX = (int)glm::floor(mouseDragEnd.x / 5);
			int tileY = gat->height - (int)glm::floor(mouseDragEnd.z / 5)+1;

			int tileMinX = (int)glm::floor(glm::min(mouseDragStart.x, mouseDragEnd.x) / 5);
			int tileMaxX = (int)glm::ceil(glm::max(mouseDragStart.x, mouseDragEnd.x) / 5);

			int tileMaxY = gat->height - (int)glm::floor(glm::min(mouseDragStart.z, mouseDragEnd.z) / 5) + 2;
			int tileMinY = gat->height - (int)glm::ceil(glm::max(mouseDragStart.z, mouseDragEnd.z) / 5) + 2;


			ImGui::Begin("Statusbar");
			ImGui::Text("Selection: (%d,%d) - (%d,%d)", tileMinX, tileMinY, tileMaxX, tileMaxY);
			ImGui::End();

			if (browEdit->selectTool == BrowEdit::SelectTool::Rectangle)
			{
				if (tileMinX >= 0 && tileMaxX < gat->width + 1 && tileMinY >= 0 && tileMaxY < gat->height + 1)
					for (int x = tileMinX; x < tileMaxX; x++)
					{
						for (int y = tileMinY; y < tileMaxY; y++)
						{
							auto cube = gat->cubes[x][y];
							verts.push_back(VertexP3T2N3(glm::vec3(5 * x,		-cube->h3 + dist, 5 * gat->height - 5 * y + 5), glm::vec2(0), cube->normal));
							verts.push_back(VertexP3T2N3(glm::vec3(5 * x,		-cube->h1 + dist, 5 * gat->height - 5 * y + 10), glm::vec2(0), cube->normal));
							verts.push_back(VertexP3T2N3(glm::vec3(5 * x + 5,	-cube->h4 + dist, 5 * gat->height - 5 * y + 5), glm::vec2(0), cube->normal));

							verts.push_back(VertexP3T2N3(glm::vec3(5 * x,		-cube->h1 + dist, 5 * gat->height - 5 * y + 10), glm::vec2(0), cube->normal));
							verts.push_back(VertexP3T2N3(glm::vec3(5 * x + 5,	-cube->h4 + dist, 5 * gat->height - 5 * y + 5), glm::vec2(0), cube->normal));
							verts.push_back(VertexP3T2N3(glm::vec3(5 * x + 5,	-cube->h2 + dist, 5 * gat->height - 5 * y + 10), glm::vec2(0), cube->normal));
						}
					}
			}
			else if (browEdit->selectTool == BrowEdit::SelectTool::Lasso) // lasso
			{
				if (tileX >= 0 && tileX < gat->width && tileY >= 0 && tileY < gat->height)
				{
					if (selectLasso.empty())
						selectLasso.push_back(glm::ivec2(tileX, tileY));
					else
						if (selectLasso[selectLasso.size() - 1] != glm::ivec2(tileX, tileY))
							selectLasso.push_back(glm::ivec2(tileX, tileY));
				}

				for (auto& t : selectLasso)
				{
					auto cube = gat->cubes[t.x][t.y];
					verts.push_back(VertexP3T2N3(glm::vec3(5 * t.x, -cube->h3 + dist, 5 * gat->height - 5 * t.y + 5), glm::vec2(0), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * t.x, -cube->h1 + dist, 5 * gat->height - 5 * t.y + 10), glm::vec2(0), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * t.x + 5, -cube->h4 + dist, 5 * gat->height - 5 * t.y + 5), glm::vec2(0), cube->normal));

					verts.push_back(VertexP3T2N3(glm::vec3(5 * t.x, -cube->h1 + dist, 5 * gat->height - 5 * t.y + 10), glm::vec2(0), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * t.x + 5, -cube->h4 + dist, 5 * gat->height - 5 * t.y + 5), glm::vec2(0), cube->normal));
					verts.push_back(VertexP3T2N3(glm::vec3(5 * t.x + 5, -cube->h2 + dist, 5 * gat->height - 5 * t.y + 10), glm::vec2(0), cube->normal));
				}
			}

			if (verts.size() > 0)
			{
				glDisable(GL_DEPTH_TEST);
				glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data);
				glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 3);
				glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 5);
				simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1, 0, 0, 0.5f));
				glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());
				glEnable(GL_DEPTH_TEST);
			}
		}
	}

	glDepthMask(1);






	fbo->unbind();
}