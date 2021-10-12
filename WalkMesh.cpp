#include "WalkMesh.hpp"

#include "read_write_chunk.hpp"

#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>

WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::vec3 > const &normals_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), normals(normals_), triangles(triangles_) {

	//construct next_vertex map (maps each edge to the next vertex in the triangle):
	next_vertex.reserve(triangles.size()*3);
	auto do_next = [this](uint32_t a, uint32_t b, uint32_t c) {
		auto ret = next_vertex.insert(std::make_pair(glm::uvec2(a,b), c));
		assert(ret.second);
	};
	for (auto const &tri : triangles) {
		do_next(tri.x, tri.y, tri.z);
		do_next(tri.y, tri.z, tri.x);
		do_next(tri.z, tri.x, tri.y);
	}

	//DEBUG: are vertex normals consistent with geometric normals?
	for (auto const &tri : triangles) {
		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];
		glm::vec3 out = glm::normalize(glm::cross(b-a, c-a));

		float da = glm::dot(out, normals[tri.x]);
		float db = glm::dot(out, normals[tri.y]);
		float dc = glm::dot(out, normals[tri.z]);

		assert(da > 0.1f && db > 0.1f && dc > 0.1f);
	}
}

//project pt to the plane of triangle a,b,c and return the barycentric weights of the projected point:
glm::vec3 barycentric_weights(glm::vec3 const &a, glm::vec3 const &b, glm::vec3 const &c, glm::vec3 const &pt) {
	//compute plane normal:
	glm::vec3 h = glm::cross(c - a, b - c);

	//compute projected, signed areas of sub-triangles formed with pt:
	float A = glm::dot(glm::cross(b - pt, c - pt), h);
	float B = glm::dot(glm::cross(c - pt, a - pt), h);
	float C = glm::dot(glm::cross(a - pt, b - pt), h);
	float S = A + B + C;

	return glm::vec3(A, B, C) / S;
}

WalkPoint WalkMesh::nearest_walk_point(glm::vec3 const &world_point) const {
	assert(!triangles.empty() && "Cannot start on an empty walkmesh");

	WalkPoint closest;
	float closest_dis2 = std::numeric_limits< float >::infinity();

	for (auto const &tri : triangles) {
		//find closest point on triangle:

		glm::vec3 const &a = vertices[tri.x];
		glm::vec3 const &b = vertices[tri.y];
		glm::vec3 const &c = vertices[tri.z];

		//get barycentric coordinates of closest point in the plane of (a,b,c):
		glm::vec3 coords = barycentric_weights(a,b,c, world_point);

		//is that point inside the triangle?
		if (coords.x >= 0.0f && coords.y >= 0.0f && coords.z >= 0.0f) {
			//yes, point is inside triangle.
			float dis2 = glm::length2(world_point - to_world_point(WalkPoint(tri, coords)));
			if (dis2 < closest_dis2) {
				closest_dis2 = dis2;
				closest.indices = tri;
				closest.weights = coords;
			}
		} else {
			//check triangle vertices and edges:
			auto check_edge = [&world_point, &closest, &closest_dis2, this](uint32_t ai, uint32_t bi, uint32_t ci) {
				glm::vec3 const &a = vertices[ai];
				glm::vec3 const &b = vertices[bi];

				//find closest point on line segment ab:
				float along = glm::dot(world_point-a, b-a);
				float max = glm::dot(b-a, b-a);
				glm::vec3 pt;
				glm::vec3 coords;
				if (along < 0.0f) {
					pt = a;
					coords = glm::vec3(1.0f, 0.0f, 0.0f);
				} else if (along > max) {
					pt = b;
					coords = glm::vec3(0.0f, 1.0f, 0.0f);
				} else {
					float amt = along / max;
					pt = glm::mix(a, b, amt);
					coords = glm::vec3(1.0f - amt, amt, 0.0f);
				}

				float dis2 = glm::length2(world_point - pt);
				if (dis2 < closest_dis2) {
					closest_dis2 = dis2;
					closest.indices = glm::uvec3(ai, bi, ci);
					closest.weights = coords;
				}
			};
			check_edge(tri.x, tri.y, tri.z);
			check_edge(tri.y, tri.z, tri.x);
			check_edge(tri.z, tri.x, tri.y);
		}
	}
	assert(closest.indices.x < vertices.size());
	assert(closest.indices.y < vertices.size());
	assert(closest.indices.z < vertices.size());
	return closest;
}

//start at 'start.weights' on triangle 'start.indices'
// move by 'step' and...
//  ...if a wall is hit, report where + when
//  ...if no wall is hit, report final point + 1.0f
void WalkMesh::walk_in_triangle(WalkPoint const &start, glm::vec3 const &step, WalkPoint *end_, float *time_) const {
	assert(end_);
	auto &end = *end_;

	assert(time_);
	auto &time = *time_;

	//get start of step, use to calculate unit norm
	glm::vec3 const& a = vertices[start.indices.x];
	glm::vec3 const& b = vertices[start.indices.y];
	glm::vec3 const& c = vertices[start.indices.z];

	glm::vec3 step_coords;
	{ 
		//wow pog we can get the projected weight with our helper :)))
		step_coords = barycentric_weights(a,b,c,to_world_point(start)+step) - start.weights;
	}
	
	//if no edge is crossed, event will just be taking the whole step:
	time = 1.0f;
	end = start;

	//check if edge crossed
	glm::vec3 move = start.weights + step_coords * time;

	//figure out which edge (if any) is crossed first.
	// set time and end appropriately.
	//account for potentially smaller minimums by checking xyz- even after updating smaller one will still be negative
	if (move.x < 0.0f) {
		assert(step_coords.x != 0);
		time = -start.weights.x / step_coords.x;
		move = start.weights + step_coords * time;
		move.x = 0.0f;
	}
	if (move.y < 0.0f) {
		assert(step_coords.y != 0);
		time = -start.weights.y / step_coords.y;
		move = start.weights + step_coords * time;
		move.y = 0.0f;
	}
	if (move.z < 0.0f) {
		assert(step_coords.z != 0);
		time = -start.weights.z / step_coords.z;
		move = start.weights + step_coords * time;
		move.z = 0.0f;
	}

	end.weights = move;

	//Remember: our convention is that when a WalkPoint is on an edge,
	// then wp.weights.z == 0.0f (so will likely need to re-order the indices)
	if (end.weights.y == 0.0f) {
		end.weights.y = end.weights.x;
		end.weights.x = end.weights.z;
		end.weights.z = 0.0f;
		unsigned int temp = end.indices.z;
		end.indices.z = end.indices.y;
		end.indices.y = end.indices.x;
		end.indices.x = temp;
	}
	else if (end.weights.x == 0.0f) {
		end.weights.x = end.weights.y;
		end.weights.y = end.weights.z;
		end.weights.z = 0.0f;
		unsigned int temp = end.indices.z;
		end.indices.z = end.indices.x;
		end.indices.x = end.indices.y;
		end.indices.y = temp;
	}
}

bool WalkMesh::cross_edge(WalkPoint const &start, WalkPoint *end_, glm::quat *rotation_) const {
	assert(end_);
	auto &end = *end_;

	assert(rotation_);
	auto &rotation = *rotation_;

	assert(start.weights.z == 0.0f); //*must* be on an edge.
	glm::uvec2 edge = glm::uvec2(start.indices);

	//check if 'edge' is a non-boundary edge by finding oppsite triangle:
	auto next_edge = next_vertex.find(edge);
	if (next_edge == next_vertex.end()) { //boundary
		end = start;
		rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		return false;
	}

	// set end's weights and indicies on opposite triangle:
	end.indices = glm::vec3(start.indices.y, start.indices.x, next_edge->second);
	end.weights = glm::vec3(start.weights.y, start.weights.x, 0); //z zero already

	/*
	// compute rotation that takes starting triangle's normal to ending triangle's normal:
	glm::vec3 const& b = vertices[start.indices.x];
	glm::vec3 const& c = vertices[start.indices.y];
	glm::vec3 const& a = vertices[start.indices.z];
	//glm::vec3 const& d = vertices[next_edge->second];

	glm::vec3 start_norm = glm::normalize(cross(c - b, a - b));
	glm::vec3 end_norm = glm::normalize(cross(d - b, c - b));

	rotation = glm::rotation(start_norm, end_norm);
	*/
	glm::vec3 const& a1 = vertices[start.indices.x];
	glm::vec3 const& b1 = vertices[start.indices.y];
	glm::vec3 const& c1 = vertices[start.indices.z];
	glm::vec3 n1_cross = glm::cross((a1 - b1), (b1 - c1));
	glm::vec3 n1 = glm::normalize(n1_cross);

	// get ending normal
	glm::vec3 const& a2 = vertices[end.indices.x];
	glm::vec3 const& b2 = vertices[end.indices.y];
	glm::vec3 const& c2 = vertices[end.indices.z];
	glm::vec3 n2_cross = glm::cross((a2 - b2), (b2 - c2));
	glm::vec3 n2 = glm::normalize(n2_cross);

	rotation = glm::rotation(n1, n2); //identity quat (wxyz init order)

	return true;
}


WalkMeshes::WalkMeshes(std::string const &filename) {
	std::ifstream file(filename, std::ios::binary);
	std::cout << "getting meshes...\n";

	std::vector< glm::vec3 > vertices;
	read_chunk(file, "p...", &vertices);

	std::vector< glm::vec3 > normals;
	read_chunk(file, "n...", &normals);

	std::vector< glm::uvec3 > triangles;
	read_chunk(file, "tri0", &triangles);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct IndexEntry {
		uint32_t name_begin, name_end;
		uint32_t vertex_begin, vertex_end;
		uint32_t triangle_begin, triangle_end;
	};

	std::vector< IndexEntry > index;
	read_chunk(file, "idxA", &index);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in walkmesh file '" << filename << "'" << std::endl;
	}

	//-----------------

	if (vertices.size() != normals.size()) {
		throw std::runtime_error("Mis-matched position and normal sizes in '" + filename + "'");
	}

	for (auto const &e : index) {
		if (!(e.name_begin <= e.name_end && e.name_end <= names.size())) {
			throw std::runtime_error("Invalid name indices in index of '" + filename + "'");
		}
		if (!(e.vertex_begin <= e.vertex_end && e.vertex_end <= vertices.size())) {
			throw std::runtime_error("Invalid vertex indices in index of '" + filename + "'");
		}
		if (!(e.triangle_begin <= e.triangle_end && e.triangle_end <= triangles.size())) {
			throw std::runtime_error("Invalid triangle indices in index of '" + filename + "'");
		}

		//copy vertices/normals:
		std::vector< glm::vec3 > wm_vertices(vertices.begin() + e.vertex_begin, vertices.begin() + e.vertex_end);
		std::vector< glm::vec3 > wm_normals(normals.begin() + e.vertex_begin, normals.begin() + e.vertex_end);

		//remap triangles:
		std::vector< glm::uvec3 > wm_triangles; wm_triangles.reserve(e.triangle_end - e.triangle_begin);
		for (uint32_t ti = e.triangle_begin; ti != e.triangle_end; ++ti) {
			if (!( (e.vertex_begin <= triangles[ti].x && triangles[ti].x < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].y && triangles[ti].y < e.vertex_end)
			    && (e.vertex_begin <= triangles[ti].z && triangles[ti].z < e.vertex_end) )) {
				throw std::runtime_error("Invalid triangle in '" + filename + "'");
			}
			wm_triangles.emplace_back(
				triangles[ti].x - e.vertex_begin,
				triangles[ti].y - e.vertex_begin,
				triangles[ti].z - e.vertex_begin
			);
		}
		
		std::string name(names.begin() + e.name_begin, names.begin() + e.name_end);
		std::cout << "mesh name: " << name << "\n";

		auto ret = meshes.emplace(name, WalkMesh(wm_vertices, wm_normals, wm_triangles));
		if (!ret.second) {
			throw std::runtime_error("WalkMesh with duplicated name '" + name + "' in '" + filename + "'");
		}

	}
}

WalkMesh const &WalkMeshes::lookup(std::string const &name) const {
	auto f = meshes.find(name);
	if (f == meshes.end()) {
		throw std::runtime_error("WalkMesh with name '" + name + "' not found.");
	}
	return f->second;
}