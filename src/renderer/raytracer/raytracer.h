#pragma once

#include "resource.h"

#include <iostream>
#include <linalg.h>
#include <memory>
#include <omp.h>
#include <random>
#include "world/camera.h"
#include "DirectXMath.h"
#include "DirectXCollision.h"

#include <set>

using namespace linalg::aliases;

namespace cg::renderer
{
	struct ray
	{
		ray(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR dir) : position(pos),
															  direction(DirectX::XMVector3Normalize(dir))
		{
			// TODO: test DirectX::XMVector3NormalizeEst
		}

		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
	};


	struct payload
	{
		float depth;
		DirectX::XMFLOAT3 position;
		color color;

		bool operator<(const payload& other) const
		{
			return depth < other.depth;
		}
	};


	template <typename VB>
	struct triangle
	{
		triangle(const VB& vertex_a, const VB& vertex_b, const VB& vertex_c);

		float3 a;
		float3 b;
		float3 c;

		float3 ba;
		float3 ca;

		float3 na;
		float3 nb;
		float3 nc;

		float3 ambient;
		float3 diffuse;
		float3 emissive;
	};


	template <typename VB>
	inline triangle<VB>::triangle(
		const VB& vertex_a, const VB& vertex_b, const VB& vertex_c)
	{
		THROW_ERROR("Not implemented yet");
	}


	template <typename VB>
	class aabb
	{
	public:
		void add_triangle(const triangle<VB> triangle);

		const std::vector<triangle<VB>>& get_triangles() const;

		bool aabb_test(const ray& ray) const;

	protected:
		std::vector<triangle<VB>> triangles;

		float3 aabb_min;
		float3 aabb_max;
	};


	struct light
	{
		float3 position;
		float3 color;
	};


	template <typename VB, typename RT>
	class raytracer
	{
	public:
		raytracer()
		{
		}

		~raytracer()
		{
		}

		void set_render_target(std::shared_ptr<resource<RT>> in_render_target);

		void clear_render_target();

		void set_viewport(size_t in_width, size_t in_height);

		void set_camera(std::shared_ptr<cg::world::camera> in_camera);

		void set_vertex_buffers(std::vector<std::shared_ptr<cg::resource<VB>>> in_vertex_buffers);

		void set_index_buffers(std::vector<std::shared_ptr<cg::resource<unsigned int>>> in_index_buffers);

		void build_acceleration_structure();

		std::vector<aabb<VB>> acceleration_structures;

		void ray_generation(size_t depth,
							size_t accumulation_num);

		bool trace_ray(const ray& ray, float max_t, float min_t, payload& payload) const;

		payload intersection_shader(const triangle<VB>& triangle, const ray& ray) const;

		std::function<payload(const ray& ray)> miss_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle, size_t depth)>
		closest_hit_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle)> any_hit_shader =
			nullptr;

		float2 get_jitter(int frame_id);

	protected:
		std::shared_ptr<cg::resource<RT>> render_target;
		std::shared_ptr<cg::resource<float3>> history;
		std::vector<std::shared_ptr<cg::resource<unsigned int>>> index_buffers;
		std::vector<std::shared_ptr<cg::resource<VB>>> vertex_buffers;

		std::shared_ptr<cg::world::camera> camera;

		size_t width = 1920;
		size_t height = 1080;
	};


	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::set_render_target(
		std::shared_ptr<resource<RT>> in_render_target)
	{
		render_target = in_render_target;
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::clear_render_target()
	{
		if (render_target)
		{
			for (size_t y = 0; y != height; ++y)
			{
				for (size_t x = 0; x != width; ++x)
				{
					render_target->item(x, y) = unsigned_color::from_float3({
						static_cast<float>(x) / width,
						static_cast<float>(y) / height,
						1.0
					});
				}
			}
		}
	}

	template <typename VB, typename RT>
	void raytracer<VB, RT>::set_index_buffers(std::vector<std::shared_ptr<cg::resource<unsigned int>>> in_index_buffers)
	{
		index_buffers = in_index_buffers;
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::set_vertex_buffers(std::vector<std::shared_ptr<cg::resource<VB>>> in_vertex_buffers)
	{
		vertex_buffers = in_vertex_buffers;
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::build_acceleration_structure()
	{
		THROW_ERROR("Not implemented yet");
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::set_viewport(size_t in_width, size_t in_height)
	{
		width = in_width;
		height = in_height;
	}

	template <typename VB, typename RT>
	void cg::renderer::raytracer<VB, RT>::set_camera(std::shared_ptr<cg::world::camera> in_camera)
	{
		camera = in_camera;
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::ray_generation(size_t depth, size_t accumulation_num)
	{
		const float h = static_cast<float>(height);
		const float w = static_cast<float>(width);
		const float minZ = camera->get_z_near();
		const float maxZ = camera->get_z_far();
		const DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
		const DirectX::XMMATRIX view = camera->get_view_matrix();
		const DirectX::XMMATRIX projection = camera->get_projection_matrix();
		const DirectX::XMVECTOR eye = camera->get_position();

		for (size_t y = 0; y != height; ++y)
		{
			for (size_t x = 0; x != width; ++x)
			{
				const float fx = static_cast<float>(x);
				const float fy = static_cast<float>(y);
				DirectX::XMVECTOR pixel = DirectX::XMVectorSet(fx, fy, 1.0f, 0.0f);
				DirectX::XMVECTOR direction = XMVector3Unproject(pixel,
																 0.0f, 0.0f, w, h,
																 0.0f, 1.0f,
																 projection, view, world);

				ray r(eye, direction);

				payload p;
				if (trace_ray(r, maxZ, minZ, p))
				{
					render_target->item(x, y) = unsigned_color::from_color(p.color);
				}
			}
		}
	}

	template <typename VB, typename RT>
	inline bool raytracer<VB, RT>::trace_ray(
		const ray& ray, float max_t, float min_t, payload& outPayload) const
	{
		std::set<payload> hits;

		for (size_t modelIdx = 0; modelIdx != index_buffers.size(); ++modelIdx)
		{
			const size_t numIndices = index_buffers.at(modelIdx)->get_number_of_elements();
			const size_t numFaces = numIndices / 3;

			for (size_t faceIdx = 0; faceIdx != numFaces; ++faceIdx)
			{
				// Extract triangle
				std::array<DirectX::XMVECTOR, 3> face;
				color faceColor = color::from_float3(float3(1.0f, 0.0f, 1.0f));
				for (size_t i = 0; i != 3; ++i)
				{
					const unsigned index = index_buffers.at(modelIdx)->item(3 * faceIdx + i);
					const vertex& vertex = vertex_buffers.at(modelIdx)->item(index);
					const DirectX::XMFLOAT3 address(vertex.x, vertex.y, vertex.z);
					face.at(i) = DirectX::XMLoadFloat3(&address);
					faceColor = color::from_float3(float3(vertex.diffuse_r, vertex.diffuse_g, vertex.diffuse_b));
				}

				float distance;
				if (DirectX::TriangleTests::Intersects(
					ray.position, ray.direction, face.at(0), face.at(1), face.at(2), distance))
				{
					if (distance >= min_t && distance <= max_t)
					{
						const DirectX::XMVECTOR hitPoint = DirectX::XMVectorAdd(
							ray.position,
							DirectX::XMVectorScale(ray.direction, distance));

						payload hit;
						hit.depth = distance;
						DirectX::XMStoreFloat3(&hit.position, hitPoint);
						hit.color = faceColor;
						//hit.color = color::from_float3(float3(&hit.position.x));

						hits.insert(hit);
					}
				}
			}
		}

		if (hits.size())
		{
			outPayload = *hits.begin();
		}
		return !hits.empty();
	}

	template <typename VB, typename RT>
	inline payload raytracer<VB, RT>::intersection_shader(
		const triangle<VB>& triangle, const ray& ray) const
	{
		THROW_ERROR("Not implemented yet");
	}

	template <typename VB, typename RT>
	float2 raytracer<VB, RT>::get_jitter(int frame_id)
	{
		THROW_ERROR("Not implemented yet");
	}

	template <typename VB>
	inline void aabb<VB>::add_triangle(const triangle<VB> triangle)
	{
		THROW_ERROR("Not implemented yet");
	}

	template <typename VB>
	inline const std::vector<triangle<VB>>& aabb<VB>::get_triangles() const
	{
		THROW_ERROR("Not implemented yet");
	}

	template <typename VB>
	inline bool aabb<VB>::aabb_test(const ray& ray) const
	{
		THROW_ERROR("Not implemented yet");
	}
} // namespace cg::renderer
