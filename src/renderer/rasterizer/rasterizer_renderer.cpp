#include "rasterizer_renderer.h"

#include "utils/resource_utils.h"


void cg::renderer::rasterization_renderer::init()
{
	//THROW_ERROR("Not implemented yet");
	render_target = std::make_shared<resource<unsigned_color>>(get_width(), get_height());
	rasterizer = std::make_shared<cg::renderer::rasterizer<vertex, unsigned_color>>();
	rasterizer->set_render_target(render_target);
	rasterizer->set_viewport(get_width(), get_height());

	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);
}

void cg::renderer::rasterization_renderer::destroy() {}

void cg::renderer::rasterization_renderer::update() {}

void cg::renderer::rasterization_renderer::render()
{
	//THROW_ERROR("Not implemented yet");
	rasterizer->clear_render_target({218, 160, 221}); // Lavender color

	auto &vertex_buffers = model->get_vertex_buffers();
	auto &index_buffers = model->get_index_buffers();
	const size_t num_render_calls = vertex_buffers.size();

	for (size_t render_idx = 0; render_idx != num_render_calls; ++render_idx) {
		rasterizer->set_vertex_buffer(vertex_buffers[render_idx]);
		rasterizer->set_index_buffer(index_buffers[render_idx]);

		rasterizer->draw(index_buffers[render_idx]->get_number_of_elements(), 0);
	}

	utils::save_resource(*render_target, settings->result_path);
}