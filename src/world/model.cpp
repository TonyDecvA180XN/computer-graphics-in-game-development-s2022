#define TINYOBJLOADER_IMPLEMENTATION

#include "model.h"

#include "utils/error_handler.h"

#include <DirectXMath.h>
#include <iostream>
#include <set>
#include <linalg.h>
#include <random>


using namespace linalg::aliases;
using namespace cg::world;

cg::world::model::model() {}

cg::world::model::~model() {}

void cg::world::model::load_obj(const std::filesystem::path& model_path)
{
	//THROW_ERROR("Not implemented yet");
	std::string error_message, warning_message;
	std::filesystem::path dir = model_path;
	dir.remove_filename();
	const bool status = LoadObj(&attrib, &shapes, &materials,
	                            &warning_message, &error_message,
	                            model_path.generic_string().c_str(),
	                            dir.generic_string().c_str(), true);

	if (warning_message.size()) {
		std::cerr << "Warning: " << warning_message << std::endl;
	}

	if (!status) {
		THROW_ERROR(error_message);
	}

	// Extract all vertices that in the file into global buffer
	const size_t num_vertices = attrib.vertices.size() / 3;
	std::vector<vertex> vertices(num_vertices);
	for (size_t i = 0; i != num_vertices; ++i) {
		vertex& current_vertex = vertices[i];
		current_vertex.x = attrib.vertices.at(3 * i + 0);
		current_vertex.y = attrib.vertices.at(3 * i + 1);
		current_vertex.z = attrib.vertices.at(3 * i + 2);

		//current_vertex.diffuse_r = attrib.colors.at(3 * i + 0);
		//current_vertex.diffuse_g = attrib.colors.at(3 * i + 1);
		//current_vertex.diffuse_b = attrib.colors.at(3 * i + 2);
	}

	// Process each shape
	for (auto & [name, mesh, lines, points] : shapes) {
		// Pick vertices from global vertex buffer and add
		// them to the local vertex buffer
		// Save local index for index buffer remapping
		std::vector<vertex> vertex_accumulator;
		std::map<unsigned int, unsigned int> index_map{};
		for (size_t i = 0; i != mesh.indices.size(); ++i) {
			unsigned int index = mesh.indices[i].vertex_index;
			if (index_map.count(index) == 0) {
				const unsigned int local_index = static_cast<unsigned int>(vertex_accumulator.size());
				vertex_accumulator.push_back(vertices[index]);

				vertex_accumulator.back().diffuse_r = materials[mesh.material_ids[i / 3]].diffuse[0];
				vertex_accumulator.back().diffuse_g = materials[mesh.material_ids[i / 3]].diffuse[1];
				vertex_accumulator.back().diffuse_b = materials[mesh.material_ids[i / 3]].diffuse[2];

				vertex_accumulator.back().ambient_r = materials[mesh.material_ids[i / 3]].ambient[0];
				vertex_accumulator.back().ambient_g = materials[mesh.material_ids[i / 3]].ambient[1];
				vertex_accumulator.back().ambient_b = materials[mesh.material_ids[i / 3]].ambient[2];

				vertex_accumulator.back().r = materials[mesh.material_ids[i / 3]].roughness;

				index_map[index] = local_index;
			}
		}

		// Create index buffer using mapped index bindings
		auto index_buffer = std::make_shared<resource<unsigned int>>(mesh.indices.size());
		for (size_t i = 0; i != mesh.indices.size(); ++i) {
			index_buffer->item(i) = index_map[mesh.indices[mesh.indices.size() - i - 1].vertex_index];
		}

		// Create vertex buffer with local only vertices
		auto vertex_buffer = std::make_shared<resource<vertex>>(vertex_accumulator.size());
		for (size_t i = 0; i != vertex_accumulator.size(); ++i) {
			vertex_buffer->item(i) = vertex_accumulator[i];
		}

		vertex_buffers.emplace_back(vertex_buffer);
		index_buffers.emplace_back(index_buffer);
	}
}


const std::vector<std::shared_ptr<cg::resource<cg::vertex>>>&
cg::world::model::get_vertex_buffers() const
{
	//THROW_ERROR("Not implemented yet");
	return vertex_buffers;
}


const std::vector<std::shared_ptr<cg::resource<unsigned int>>>&
cg::world::model::get_index_buffers() const
{
	//THROW_ERROR("Not implemented yet");
	return index_buffers;
}

std::vector<std::filesystem::path>
cg::world::model::get_per_shape_texture_files() const
{
	THROW_ERROR("Not implemented yet");
}


const DirectX::XMMATRIX cg::world::model::get_world_matrix() const
{
	//THROW_ERROR("Not implemented yet");

	// Test random angles
	//std::mt19937 rng;
	//std::uniform_real_distribution<float> dist(-DirectX::XM_PI, DirectX::XM_PI);

	return DirectX::XMMatrixRotationY(0);
}
