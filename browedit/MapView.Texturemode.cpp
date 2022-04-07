#define IMGUI_DEFINE_MATH_OPERATORS
#include "MapView.h"
#include <browedit/shaders/SimpleShader.h>
#include <browedit/gl/FBO.h>
#include <browedit/Node.h>
#include <browedit/Map.h>
#include <browedit/gl/Texture.h>
#include <browedit/components/Gnd.h>
#include <browedit/components/GndRenderer.h>
#include <browedit/actions/GroupAction.h>
#include <browedit/actions/TileNewAction.h>
#include <browedit/actions/CubeTileChangeAction.h>

#include <imgui_internal.h>

void MapView::postRenderTextureMode(BrowEdit* browEdit)
{
	auto gnd = map->rootNode->getComponent<Gnd>();
	auto gndRenderer = map->rootNode->getComponent<GndRenderer>();


	simpleShader->use();
	simpleShader->setUniform(SimpleShader::Uniforms::projectionMatrix, nodeRenderContext.projectionMatrix);
	simpleShader->setUniform(SimpleShader::Uniforms::viewMatrix, nodeRenderContext.viewMatrix);
	simpleShader->setUniform(SimpleShader::Uniforms::modelMatrix, glm::mat4(1.0f));
	simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 0.0f);


	fbo->bind();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisable(GL_TEXTURE_2D);

	if (textureGridVbo->size() == 0 || textureGridDirty)
	{
		textureGridDirty = false;
		buildTextureGrid();
	}
	if(textureGridVbo->size() > 0 && snapToGrid)
	{
		simpleShader->setUniform(SimpleShader::Uniforms::modelMatrix, glm::mat4(1.0f));
		simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		glLineWidth(2);
		textureGridVbo->bind();
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		glDisableVertexAttribArray(3);
		glDisableVertexAttribArray(4); //TODO: vao
		glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2), (void*)(0 * sizeof(float)));
		glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2), (void*)(3 * sizeof(float)));
		glDrawArrays(GL_LINES, 0, (int)textureGridVbo->size());
		textureGridVbo->unBind();
	}

	auto mouse3D = gnd->rayCast(mouseRay, viewEmptyTiles);
	glm::vec2 uvSize = textureEditUv2 - textureEditUv1;
	glm::vec2 uv1 = textureEditUv1; //(0,0)
	glm::vec2 uv4 = textureEditUv2; //(1,1)

	glm::vec2 uv2(uv4.x, uv1.y);
	glm::vec2 uv3(uv1.x, uv4.y);

	if (textureBrushFlipD)
		std::swap(uv1, uv4); // different from texturebrush window due to flipped UV

	if (textureBrushFlipH)
	{
		uv1.x = 1 - uv1.x;
		uv2.x = 1 - uv2.x;
		uv3.x = 1 - uv3.x;
		uv4.x = 1 - uv4.x;
	}
	if (textureBrushFlipV)
	{
		uv1.y = 1 - uv1.y;
		uv2.y = 1 - uv2.y;
		uv3.y = 1 - uv3.y;
		uv4.y = 1 - uv4.y;
	}
	glm::vec2 xInc = (uv2 - uv1) / (float)textureBrushWidth;
	glm::vec2 yInc = (uv3 - uv1) / (float)textureBrushHeight;

	if (textureStamp.size() == 0 && !ImGui::GetIO().KeyShift)
	{
		if (mouse3D != glm::vec3(0, 0, 0) && !(ImGui::IsMouseDown(0) && ImGui::GetIO().KeyShift))
		{
			glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 10), (gnd->height - (int)glm::floor(mouse3D.z) / 10));

			std::vector<VertexP3T2N3> verts;
			float dist = 0.002f * cameraDistance;

			glm::ivec2 topleft = tileHovered - glm::ivec2(textureBrushWidth / 2, textureBrushHeight / 2);
			for (int x = 0; x < textureBrushWidth; x++)
			{
				for (int y = 0; y < textureBrushHeight; y++)
				{
					if (topleft.x + x >= gnd->width || topleft.x + x < 0 || topleft.y + y >= gnd->height || topleft.y + y < 0)
						continue;
					auto cube = gnd->cubes[topleft.x + x][topleft.y + y];

					glm::vec2 uv1_ = uv1 + xInc * (float)x + yInc * (float)y;
					glm::vec2 uv2_ = uv1_ + xInc;
					glm::vec2 uv3_ = uv1_ + yInc;
					glm::vec2 uv4_ = uv1_ + xInc + yInc;

					uv1_.y = 1 - uv1_.y;
					uv2_.y = 1 - uv2_.y;
					uv3_.y = 1 - uv3_.y;
					uv4_.y = 1 - uv4_.y;

					verts.push_back(VertexP3T2N3(glm::vec3(10 * (topleft.x + x), -cube->h3 + dist, 10 * gnd->height - 10 * (topleft.y + y)), uv3_, cube->normals[2]));
					verts.push_back(VertexP3T2N3(glm::vec3(10 * (topleft.x + x) + 10, -cube->h2 + dist, 10 * gnd->height - 10 * (topleft.y + y) + 10), uv2_, cube->normals[1]));
					verts.push_back(VertexP3T2N3(glm::vec3(10 * (topleft.x + x) + 10, -cube->h4 + dist, 10 * gnd->height - 10 * (topleft.y + y)), uv4_, cube->normals[3]));

					verts.push_back(VertexP3T2N3(glm::vec3(10 * (topleft.x + x), -cube->h1 + dist, 10 * gnd->height - 10 * (topleft.y + y) + 10), uv1_, cube->normals[0]));
					verts.push_back(VertexP3T2N3(glm::vec3(10 * (topleft.x + x), -cube->h3 + dist, 10 * gnd->height - 10 * (topleft.y + y)), uv3_, cube->normals[2]));
					verts.push_back(VertexP3T2N3(glm::vec3(10 * (topleft.x + x) + 10, -cube->h2 + dist, 10 * gnd->height - 10 * (topleft.y + y) + 10), uv2_, cube->normals[1]));
				}
			}
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			glDisableVertexAttribArray(4); //TODO: vao
			glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data);
			glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 3);
			glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 5);
			gndRenderer->textures[textureSelected]->bind();
			simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1, 1, 1, 0.5f));
			simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 1.0f);
			simpleShader->setUniform(SimpleShader::Uniforms::lightMin, 1.0f);
			glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());
			simpleShader->setUniform(SimpleShader::Uniforms::lightMin, 0.5f);
			simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 0.0f);
		}
	}
	else if (!ImGui::GetIO().KeyShift) //imagestamp
	{
		glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 10), (gnd->height - (int)glm::floor(mouse3D.z) / 10));
		glm::ivec2 topleft = tileHovered - glm::ivec2(textureStamp.size() / 2, textureStamp[0].size() / 2);
		float dist = 0.002f * cameraDistance;
		std::map<int, std::vector<VertexP3T2N3>> verts;
		for (auto xx = 0; xx < textureStamp.size(); xx++)
		{
			for (auto yy = 0; yy < textureStamp[xx].size(); yy++)
			{
				int x = topleft.x + xx;
				int y = topleft.y + yy;
				if (x < 0 || x >= gnd->width || y < 0 || y >= gnd->height)
					continue;
				auto tile = textureStamp[xx][yy];
				if (!tile)
					continue;
				auto cube = gnd->cubes[x][y];
				verts[tile->textureIndex].push_back(VertexP3T2N3(glm::vec3(10 * x, -cube->h3 + dist, 10 * gnd->height - 10 * y), tile->v3, cube->normals[2]));
				verts[tile->textureIndex].push_back(VertexP3T2N3(glm::vec3(10 * x, -cube->h1 + dist, 10 * gnd->height - 10 * y + 10), tile->v1, cube->normals[0]));
				verts[tile->textureIndex].push_back(VertexP3T2N3(glm::vec3(10 * x + 10, -cube->h4 + dist, 10 * gnd->height - 10 * y), tile->v4, cube->normals[3]));

				verts[tile->textureIndex].push_back(VertexP3T2N3(glm::vec3(10 * x, -cube->h1 + dist, 10 * gnd->height - 10 * y + 10), tile->v1, cube->normals[0]));
				verts[tile->textureIndex].push_back(VertexP3T2N3(glm::vec3(10 * x + 10, -cube->h4 + dist, 10 * gnd->height - 10 * y), tile->v4, cube->normals[3]));
				verts[tile->textureIndex].push_back(VertexP3T2N3(glm::vec3(10 * x + 10, -cube->h2 + dist, 10 * gnd->height - 10 * y + 10), tile->v2, cube->normals[1]));
			}
		}
		simpleShader->setUniform(SimpleShader::Uniforms::lightMin, 1.0f);
		simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 1.0f);
		simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1, 1, 1, 0.5f));
		for (auto texVerts : verts)
		{
			gndRenderer->textures[texVerts.first]->bind();
			glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), texVerts.second[0].data);
			glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), texVerts.second[0].data + 3);
			glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), texVerts.second[0].data + 5);
			glDrawArrays(GL_TRIANGLES, 0, (int)texVerts.second.size());
		}
		simpleShader->setUniform(SimpleShader::Uniforms::textureFac, 0.0f);
		simpleShader->setUniform(SimpleShader::Uniforms::lightMin, 0.5f);
	}




	if (hovered && mouse3D != glm::vec3(0, 0, 0))
	{
		if (ImGui::IsMouseDown(0) && ImGui::GetIO().KeyShift)
		{//dragging mouse to show preview
			if (!mouseDown)
				mouseDragStart = mouse3D;
			mouseDown = true;
			auto mouseDragEnd = mouse3D;
			std::vector<VertexP3T2N3> verts;
			float dist = 0.002f * cameraDistance;

			int tileMinX = (int)glm::floor(glm::min(mouseDragStart.x, mouseDragEnd.x) / 10);
			int tileMaxX = (int)glm::ceil(glm::max(mouseDragStart.x, mouseDragEnd.x) / 10);

			int tileMaxY = gnd->height - (int)glm::floor(glm::min(mouseDragStart.z, mouseDragEnd.z) / 10) + 1;
			int tileMinY = gnd->height - (int)glm::ceil(glm::max(mouseDragStart.z, mouseDragEnd.z) / 10) + 1;


			ImGui::Begin("Statusbar");
			ImGui::Text("Selection: (%d,%d) - (%d,%d)", tileMinX, tileMinY, tileMaxX, tileMaxY);
			ImGui::End();

			if (tileMinX >= 0 && tileMaxX < gnd->width + 1 && tileMinY >= 0 && tileMaxY < gnd->height + 1)
				for (int x = tileMinX; x < tileMaxX; x++)
				{
					for (int y = tileMinY; y < tileMaxY; y++)
					{
						auto cube = gnd->cubes[x][y];
						verts.push_back(VertexP3T2N3(glm::vec3(10 * x, -cube->h3 + dist, 10 * gnd->height - 10 * y), glm::vec2(0), cube->normals[2]));
						verts.push_back(VertexP3T2N3(glm::vec3(10 * x, -cube->h1 + dist, 10 * gnd->height - 10 * y + 10), glm::vec2(0), cube->normals[0]));
						verts.push_back(VertexP3T2N3(glm::vec3(10 * x + 10, -cube->h4 + dist, 10 * gnd->height - 10 * y), glm::vec2(0), cube->normals[3]));

						verts.push_back(VertexP3T2N3(glm::vec3(10 * x, -cube->h1 + dist, 10 * gnd->height - 10 * y + 10), glm::vec2(0), cube->normals[0]));
						verts.push_back(VertexP3T2N3(glm::vec3(10 * x + 10, -cube->h4 + dist, 10 * gnd->height - 10 * y), glm::vec2(0), cube->normals[3]));
						verts.push_back(VertexP3T2N3(glm::vec3(10 * x + 10, -cube->h2 + dist, 10 * gnd->height - 10 * y + 10), glm::vec2(0), cube->normals[1]));
					}
				}
			if (verts.size() > 0)
			{
				glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data);
				glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 3);
				glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(VertexP3T2N3), verts[0].data + 5);
				simpleShader->setUniform(SimpleShader::Uniforms::color, glm::vec4(1, 0, 0, 0.5f));
				glDrawArrays(GL_TRIANGLES, 0, (int)verts.size());
			}
		}
		else if (ImGui::IsMouseReleased(0) && mouseDown)
		{ //release mouse to create texturestamp
			mouseDown = false;
			for (auto r : textureStamp)
				for (auto rr : r)
					delete rr;
			textureStamp.clear();
			if (ImLengthSqr(ImGui::GetMouseDragDelta(0)) > 3)
			{
				auto mouseDragEnd = mouse3D;
				mouseDown = false;
				int tileMinX = (int)glm::floor(glm::min(mouseDragStart.x, mouseDragEnd.x) / 10);
				int tileMaxX = (int)glm::ceil(glm::max(mouseDragStart.x, mouseDragEnd.x) / 10);

				int tileMaxY = gnd->height - (int)glm::floor(glm::min(mouseDragStart.z, mouseDragEnd.z) / 10) + 1;
				int tileMinY = gnd->height - (int)glm::ceil(glm::max(mouseDragStart.z, mouseDragEnd.z) / 10) + 1;

				if (tileMinX >= 0 && tileMaxX < gnd->width + 1 && tileMinY >= 0 && tileMaxY < gnd->height + 1)
					for (int x = tileMinX; x < tileMaxX; x++)
					{
						std::vector<Gnd::Tile*> col;
						for (int y = tileMinY; y < tileMaxY; y++)
						{
							auto cube = gnd->cubes[x][y];
							if (cube->tileUp == -1)
								col.push_back(nullptr);
							else
								col.push_back(new Gnd::Tile(*gnd->tiles[cube->tileUp]));
						}
						textureStamp.push_back(col);
					}
			}

		}
		else if (ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyShift)
		{
			if (textureStamp.size() == 0)
			{
				auto ga = new GroupAction();

				glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 10), (gnd->height - (int)glm::floor(mouse3D.z) / 10));
				glm::ivec2 topleft = tileHovered - glm::ivec2(textureBrushWidth / 2, textureBrushHeight / 2);
				int id = (int)gnd->tiles.size();
				for (int x = 0; x < textureBrushWidth; x++)
				{
					for (int y = 0; y < textureBrushHeight; y++)
					{
						if (topleft.x + x >= gnd->width || topleft.x + x < 0 || topleft.y + y >= gnd->height || topleft.y + y < 0)
							continue;
						auto cube = gnd->cubes[topleft.x + x][topleft.y + y];

						Gnd::Tile* t = new Gnd::Tile();

						t->v1 = uv1 + xInc * (float)x + yInc * (float)y;
						t->v2 = t->v1 + xInc;
						t->v3 = t->v1 + yInc;
						t->v4 = t->v1 + xInc + yInc;

						for (int i = 0; i < 4; i++)
							t->texCoords[i].y = 1 - t->texCoords[i].y;

						t->color = glm::ivec4(255, 255, 255, 255);
						t->textureIndex = textureSelected;
						t->lightmapIndex = 0;
						if (textureBrushKeepShadow && cube->tileUp != -1)
							t->lightmapIndex = gnd->tiles[cube->tileUp]->lightmapIndex;
						if (textureBrushKeepColor && cube->tileUp != -1)
							t->color = gnd->tiles[cube->tileUp]->color;

						ga->addAction(new TileNewAction(t));
						ga->addAction(new CubeTileChangeAction(cube, id, cube->tileFront, cube->tileSide));
						cube->tileUp = id;
						id++;
					}
				}
				textureGridDirty = true;
				map->doAction(ga, browEdit);
			}
			else 
			{//do texture stamping
				auto ga = new GroupAction();

				glm::ivec2 tileHovered((int)glm::floor(mouse3D.x / 10), (gnd->height - (int)glm::floor(mouse3D.z) / 10));
				glm::ivec2 topleft = tileHovered - glm::ivec2(textureStamp.size() / 2, textureStamp[0].size() / 2);
				int id = (int)gnd->tiles.size();
				for (auto xx = 0; xx < textureStamp.size(); xx++)
				{
					for (int yy = 0; yy < textureStamp[xx].size(); yy++)
					{
						int x = topleft.x + xx;
						int y = topleft.y + yy;
						if (x < 0 || x >= gnd->width || y < 0 || y >= gnd->height)
							continue;
						auto cube = gnd->cubes[x][y];
						Gnd::Tile* t = new Gnd::Tile(*textureStamp[xx][yy]);
						if (textureBrushKeepShadow && cube->tileUp != -1)
							t->lightmapIndex = gnd->tiles[cube->tileUp]->lightmapIndex;
						if (textureBrushKeepColor && cube->tileUp != -1)
							t->color = gnd->tiles[cube->tileUp]->color;
						ga->addAction(new TileNewAction(t));
						ga->addAction(new CubeTileChangeAction(cube, id, cube->tileFront, cube->tileSide));
						cube->tileUp = id;
						id++;
					}
				}
				textureGridDirty = true;
				map->doAction(ga, browEdit);
			}
		}

	}


	fbo->unbind();

}


















void MapView::buildTextureGrid()
{
	std::vector<VertexP3T2> verts;

	auto gnd = map->rootNode->getComponent<Gnd>();
	for (int x = 1; x < gnd->width - 1; x++)
	{
		for (int y = 1; y < gnd->height - 1; y++)
		{
			{
				Gnd::Tile* t1 = NULL;
				Gnd::Tile* t2 = NULL;
				if (gnd->cubes[x][y]->tileUp != -1)
					t1 = gnd->tiles[gnd->cubes[x][y]->tileUp];
				if (gnd->cubes[x + 1][y]->tileUp != -1)
					t2 = gnd->tiles[gnd->cubes[x + 1][y]->tileUp];

				bool drawLine = false;
				if ((t1 == NULL) != (t2 == NULL)) // NULL next to a tile
					drawLine = true;
				else if (t1 == NULL)
					drawLine = false; // both tiles are NULL
				else if (t1->textureIndex != t2->textureIndex) // 2 different textures
					drawLine = true;
				else if (glm::distance(t1->v2, t2->v1) > 0.1 || glm::distance(t1->v4, t2->v3) > 0.1)
					drawLine = true;

				if (drawLine)
				{
					verts.push_back(VertexP3T2(glm::vec3(10 * x + 10, -gnd->cubes[x][y]->h4 + 0.1f, 10 * gnd->height - 10 * y), glm::vec2()));
					verts.push_back(VertexP3T2(glm::vec3(10 * x + 10, -gnd->cubes[x][y]->h2 + 0.1f, 10 * gnd->height - 10 * y + 10), glm::vec2()));
				}
			}

			{
				Gnd::Tile* t1 = NULL;
				Gnd::Tile* t2 = NULL;
				if (gnd->cubes[x][y]->tileUp != -1)
					t1 = gnd->tiles[gnd->cubes[x][y]->tileUp];
				if (gnd->cubes[x - 1][y]->tileUp != -1)
					t2 = gnd->tiles[gnd->cubes[x - 1][y]->tileUp];

				bool drawLine = false;
				if ((t1 == NULL) != (t2 == NULL)) // NULL next to a tile
					drawLine = true;
				else if (t1 == NULL)
					drawLine = false; // both tiles are NULL
				else if (t1->textureIndex != t2->textureIndex) // 2 different textures
					drawLine = true;
				else if (glm::distance(t1->v1, t2->v2) > 0.1f || glm::distance(t1->v3, t2->v4) > 0.1f)
					drawLine = true;

				if (drawLine)
				{
					verts.push_back(VertexP3T2(glm::vec3(10 * x, -gnd->cubes[x][y]->h3 + 0.1f, 10 * gnd->height - 10 * y), glm::vec2()));
					verts.push_back(VertexP3T2(glm::vec3(10 * x, -gnd->cubes[x][y]->h1 + 0.1f, 10 * gnd->height - 10 * y + 10), glm::vec2()));
				}
			}

			{
				Gnd::Tile* t1 = NULL;
				Gnd::Tile* t2 = NULL;
				if (gnd->cubes[x][y]->tileUp != -1)
					t1 = gnd->tiles[gnd->cubes[x][y]->tileUp];
				if (gnd->cubes[x][y + 1]->tileUp != -1)
					t2 = gnd->tiles[gnd->cubes[x][y + 1]->tileUp];

				bool drawLine = false;
				if ((t1 == NULL) != (t2 == NULL)) // NULL next to a tile
					drawLine = true;
				else if (t1 == NULL)
					drawLine = false; // both tiles are NULL
				else if (t1->textureIndex != t2->textureIndex) // 2 different textures
					drawLine = true;
				else if (glm::distance(t1->v3, t2->v1) > 0.1 || glm::distance(t1->v4, t2->v2) > 0.1)
					drawLine = true;

				if (drawLine)
				{
					verts.push_back(VertexP3T2(glm::vec3(10 * x, -gnd->cubes[x][y]->h3 + 0.1f, 10 * gnd->height - 10 * y), glm::vec2()));
					verts.push_back(VertexP3T2(glm::vec3(10 * x + 10, -gnd->cubes[x][y]->h4 + 0.1f, 10 * gnd->height - 10 * y), glm::vec2()));
				}
			}

			{
				Gnd::Tile* t1 = NULL;
				Gnd::Tile* t2 = NULL;
				if (gnd->cubes[x][y]->tileUp != -1)
					t1 = gnd->tiles[gnd->cubes[x][y]->tileUp];
				if (gnd->cubes[x][y - 1]->tileUp != -1)
					t2 = gnd->tiles[gnd->cubes[x][y - 1]->tileUp];

				bool drawLine = false;
				if ((t1 == NULL) != (t2 == NULL)) // NULL next to a tile
					drawLine = true;
				else if (t1 == NULL)
					drawLine = false; // both tiles are NULL
				else if (t1->textureIndex != t2->textureIndex) // 2 different textures
					drawLine = true;
				else if (glm::distance(t1->v1, t2->v3) > 0.1f || glm::distance(t1->v2, t2->v4) > 0.1f)
					drawLine = true;

				if (drawLine)
				{
					verts.push_back(VertexP3T2(glm::vec3(10 * x, -gnd->cubes[x][y]->h1 + 0.1f, 10 * gnd->height - 10 * y + 10), glm::vec2()));
					verts.push_back(VertexP3T2(glm::vec3(10 * x + 10, -gnd->cubes[x][y]->h2 + 0.1f, 10 * gnd->height - 10 * y + 10), glm::vec2()));
				}
			}
		}
	}
	textureGridVbo->setData(verts, GL_STATIC_DRAW);
}