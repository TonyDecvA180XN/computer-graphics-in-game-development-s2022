#include "raytracer_renderer.h"

#include "utils/resource_utils.h"

#include <iostream>


void cg::renderer::ray_tracing_renderer::init()
{
	// Make camera
	camera = std::make_shared<cg::world::camera>();
	camera->set_position(float3(settings->camera_position.data()));
	camera->set_angle_of_view(settings->camera_angle_of_view);
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_theta(settings->camera_theta);
	camera->set_phi(settings->camera_phi);
	camera->set_z_near(settings->camera_z_near);
	camera->set_z_far(settings->camera_z_far);

	// Make render target
	render_target = std::make_shared<resource<unsigned_color>>(settings->width, settings->height);

	// Load model from file
	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);

	// Make raytracer
	ray_tracer = std::make_shared<cg::renderer::raytracer<vertex, unsigned_color>>();
	ray_tracer->set_viewport(settings->width, settings->height);
	ray_tracer->set_render_target(render_target);
	ray_tracer->set_camera(camera);
}

void cg::renderer::ray_tracing_renderer::destroy() {}

void cg::renderer::ray_tracing_renderer::update() {}

void cg::renderer::ray_tracing_renderer::render()
{
	ray_tracer->clear_render_target();

	auto& vertexBuffers = model->get_vertex_buffers();
	auto& indexBuffers = model->get_index_buffers();

	const size_t numShapes = vertexBuffers.size();

	ray_tracer->set_vertex_buffers(vertexBuffers);
	ray_tracer->set_index_buffers(indexBuffers);

	ray_tracer->build_acceleration_structure();

	ray_tracer->ray_generation(0, 0);

	utils::save_resource(*render_target, settings->result_path);
}