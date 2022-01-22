#define TINYOBJLOADER_IMPLEMENTATION

#include "model.h"

#include "utils/error_handler.h"

#include <iostream>
#include <linalg.h>


using namespace linalg::aliases;
using namespace cg::world;

cg::world::model::model() {}

cg::world::model::~model() {}

void cg::world::model::load_obj(const std::filesystem::path& model_path)
{
	//THROW_ERROR("Not implemented yet");
	std::string error_message, warning_message;
	bool status = LoadObj(&attrib, &shapes, &materials,
		&warning_message, &error_message,
		model_path.generic_string().c_str(),
		nullptr, true);

	if (warning_message.size()) {
		std::cerr << "Warning: " << warning_message << std::endl;
	}

	if (!status) {
		THROW_ERROR(error_message);
	}

	// Extract all vertices
	const size_t num_vertices = attrib.vertices.size() / 3;
	std::vector<vertex> vertices(num_vertices);
	for (size_t i = 0; i != num_vertices; ++i) {
		vertex& current_vertex = vertices[i];
		current_vertex.x = attrib.vertices.at(3 * i + 0);
		current_vertex.y = attrib.vertices.at(3 * i + 1);
		current_vertex.z = attrib.vertices.at(3 * i + 2);
	}

	// Process each shape
	for (auto & [name, mesh, lines, points] : shapes) {
		std::vector<tinyobj::index_t>& shape_indices = mesh.indices;

		// Generate vertex buffer & index buffer
		auto vertex_buffer = std::make_shared<resource<vertex>>(shape_indices.size());
		auto index_buffer = std::make_shared<resource<unsigned int>>(shape_indices.size());
		for (size_t i = 0; i != shape_indices.size(); ++i) {
			index_buffer->item(i) = shape_indices[i].vertex_index;
			vertex_buffer->item(i) = vertices[index_buffer->item(i)];
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


const float4x4 cg::world::model::get_world_matrix() const
{
	THROW_ERROR("Not implemented yet");
}
