#include "rasterizer_renderer.h"

#include "utils/resource_utils.h"


void cg::renderer::rasterization_renderer::init()
{
	//THROW_ERROR("Not implemented yet");
	render_target = std::make_shared<resource<unsigned_color>>(get_width(), get_height());
	rasterizer = std::make_shared<cg::renderer::rasterizer<vertex, unsigned_color>>();
	rasterizer->set_render_target(render_target);
	rasterizer->set_viewport(get_width(), get_height());
}

void cg::renderer::rasterization_renderer::destroy() {}

void cg::renderer::rasterization_renderer::update() {}

void cg::renderer::rasterization_renderer::render()
{
	//THROW_ERROR("Not implemented yet");
	rasterizer->clear_render_target({218, 160, 221}); // Lavender color

	utils::save_resource(*render_target, settings->result_path);
}