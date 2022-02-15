#pragma once

#include "resource.h"

#include <cmath>
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

template<class T>
bool IsEqual(const T v1, const T v2, const T tolerance = 0.001)
{
	return std::abs(v1 - v2) <= tolerance;
}

namespace DirectX
{
	inline XMVECTOR XMTriangleAreaTwice(FXMVECTOR a, FXMVECTOR b)
	{
		return XMVector3Length(XMVector3Cross(a, b));
	}

	inline XMVECTOR XMFindBarycentric(FXMVECTOR p, FXMVECTOR v0, FXMVECTOR v1, GXMVECTOR v2)
	{
		XMVECTOR v0v1 = XMVectorSubtract(v1, v0);
		XMVECTOR v0v2 = XMVectorSubtract(v2, v0);
		XMVECTOR area = XMTriangleAreaTwice(v0v1, v0v2);

		XMVECTOR pv0 = XMVectorSubtract(v0, p);
		XMVECTOR pv1 = XMVectorSubtract(v1, p);
		XMVECTOR pv2 = XMVectorSubtract(v2, p);

		XMVECTOR result = XMVectorSet(XMVectorGetX(XMVectorDivide(XMTriangleAreaTwice(pv1, pv2), area)),
									  XMVectorGetX(XMVectorDivide(XMTriangleAreaTwice(pv0, pv2), area)),
									  XMVectorGetX(XMVectorDivide(XMTriangleAreaTwice(pv0, pv1), area)),
									  0.0f);
		return result;
	}

	inline XMVECTOR XMVectorDotUnsigned(FXMVECTOR a, FXMVECTOR b)
	{
		return XMVectorClamp(XMVector3Dot(a, b), XMVectorZero(), XMVectorSplatOne());
	}
}

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
		vertex point;

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
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR specular;
		DirectX::XMVECTOR duffuse;
		DirectX::XMVECTOR ambient;
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

		void set_camera(std::shared_ptr<world::camera> in_camera);

		void set_vertex_buffers(std::vector<std::shared_ptr<resource<VB>>> in_vertex_buffers);

		void set_index_buffers(std::vector<std::shared_ptr<resource<unsigned int>>> in_index_buffers);

		void build_acceleration_structure();

		std::vector<aabb<VB>> acceleration_structures;

		void ray_generation(size_t depth,
							size_t accumulation_num);

		bool trace_ray(const ray& ray, float max_t, float min_t, payload& payload, bool bIsShadowRay = false) const;

		payload intersection_shader(const triangle<VB>& triangle, const ray& ray) const;

		std::function<payload(const ray& ray)> miss_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle, size_t depth)>
		closest_hit_shader = nullptr;
		std::function<payload(const ray& ray, payload& payload, const triangle<VB>& triangle)> any_hit_shader =
			nullptr;

		bool render_floor_grid(size_t x, size_t y, DirectX::FXMVECTOR eye, DirectX::FXMVECTOR dir);
		bool render_axes(size_t x, size_t y, DirectX::FXMVECTOR eye, DirectX::FXMVECTOR dir);
		bool render_sky_grid(size_t x, size_t y, DirectX::FXMVECTOR eye, DirectX::FXMVECTOR dir);

		float2 get_jitter(int frame_id);

	protected:
		std::shared_ptr<resource<RT>> render_target;
		std::shared_ptr<resource<float3>> history;
		std::vector<std::shared_ptr<resource<unsigned int>>> index_buffers;
		std::vector<std::shared_ptr<resource<VB>>> vertex_buffers;

		std::shared_ptr<world::camera> camera;

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
	void raytracer<VB, RT>::set_index_buffers(std::vector<std::shared_ptr<resource<unsigned int>>> in_index_buffers)
	{
		index_buffers = in_index_buffers;
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::set_vertex_buffers(std::vector<std::shared_ptr<resource<VB>>> in_vertex_buffers)
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
	void raytracer<VB, RT>::set_camera(std::shared_ptr<world::camera> in_camera)
	{
		camera = in_camera;
	}

	template <typename VB, typename RT>
	inline void raytracer<VB, RT>::ray_generation(size_t depth, size_t accumulation_num)
	{
		using namespace DirectX;

		const float h = static_cast<float>(height);
		const float w = static_cast<float>(width);
		const float minZ = camera->get_z_near();
		const float maxZ = camera->get_z_far();
		const XMMATRIX world = XMMatrixIdentity();
		const XMMATRIX view = camera->get_view_matrix();
		const XMMATRIX projection = camera->get_projection_matrix();
		const XMVECTOR eye = camera->get_position();

		std::vector<light> lights;
		lights.push_back({
			XMVectorSet(0.0f, 1.925f, 0.0f, 1.0f),
			XMVectorSet(0.25f, 0.25f, 0.25f, 1.0f),
			XMVectorSet(0.75f, 0.75f, 0.75f, 1.0f),
			XMVectorSet(0.4f, 0.4f, 0.4f, 1.0f)
		});

		for (size_t y = 0; y != height; ++y)
		{
			for (size_t x = 0; x != width; ++x)
			{
				const float fx = static_cast<float>(x);
				const float fy = static_cast<float>(y);
				const XMVECTOR pixel = XMVectorSet(fx, fy, 1.0f, 0.0f);
				XMVECTOR cameraRay = XMVector3Normalize(XMVector3Unproject(pixel,
																		   0.0f, 0.0f, w, h,
																		   0.0f, 1.0f,
																		   projection, view, world));
				ray r(eye, cameraRay);

				payload p;
				if (trace_ray(r, maxZ, minZ, p))
				{
					// Use this switchers to play with parameters
					constexpr bool USE_BLINN_LIGHTING = false;
					constexpr bool USE_AMBIENT = true;
					constexpr bool USE_DIFFUSE = true;
					constexpr bool USE_SPECULAR = true;

					XMVECTOR totalIntensity = XMVectorZero();
					for (const light& l : lights)
					{
						const XMVECTOR address = XMLoadFloat3(&p.point.position);
						const XMVECTOR surfaceNormal = XMLoadFloat3(&p.point.normal);
						const XMVECTOR lightVector = XMVectorSubtract(l.position, address);
						const XMVECTOR lightDir = XMVector3Normalize(lightVector);
						const XMVECTOR incident = XMVectorScale(lightDir, -1.0f);
						const XMVECTOR reflectedLight = XMVector3Reflect(incident, surfaceNormal);
						const XMVECTOR cameraDir = XMVector3Normalize(XMVectorSubtract(eye, address));
						XMVECTOR shininess = XMVectorReplicate(p.point.shininess);
						XMVECTOR shadow = XMVectorSplatOne();

						if (USE_AMBIENT)
						{
							// add ambient component
							const XMVECTOR materialAmbient = XMLoadFloat3(&p.point.ambient);
							const XMVECTOR ambientComponent = XMColorModulate(l.ambient, materialAmbient);
							totalIntensity = XMVectorAdd(totalIntensity, ambientComponent);
						}

						// skip backfaces during lighting
						if (XMVectorGetX(XMVector3Dot(lightDir, surfaceNormal)) < 0.0f)
						{
							continue;
						}

						// trace shadow
						const XMVECTOR offsetPosition = XMVectorAdd(address, XMVectorScale(surfaceNormal, 0.001f));
						ray lightRay(address, lightDir);
						payload shadowPayload;
						const bool bIsShadow = trace_ray(lightRay, XMVectorGetX(XMVector3Length(lightVector)), 0.0001f,
														 shadowPayload, true);
						if (bIsShadow)
						{
							shadow = XMVectorReplicate(0.5f);
							shadow = XMVectorClamp(shadow, XMVectorZero(), XMVectorSplatOne());
						}

						if (USE_DIFFUSE)
						{
							// add diffuse component
							const XMVECTOR materialDiffuse = XMLoadFloat3(&shadowPayload.point.diffuse);
							XMVECTOR diffuseComponent = XMVectorDotUnsigned(lightDir, surfaceNormal);
							diffuseComponent = XMColorModulate(diffuseComponent, l.duffuse);
							diffuseComponent = XMColorModulate(diffuseComponent, shadow);
							diffuseComponent = XMColorModulate(diffuseComponent, materialDiffuse);

							totalIntensity = XMVectorAdd(totalIntensity, diffuseComponent);
						}

						// add specular component
						if (!bIsShadow && USE_SPECULAR)
						{
							// Cornell box does not have material specular value
							//const XMVECTOR materialSpecular = XMLoadFloat3(&p.point.specular);
							const XMVECTOR materialSpecular = XMVectorSplatOne();
							XMVECTOR specularComponent;
							if (USE_BLINN_LIGHTING)
							{
								shininess = XMVectorScale(shininess, 0.25f);
								const XMVECTOR halfDir = XMVector3Normalize(XMVectorAdd(lightDir, cameraDir));
								specularComponent = XMVectorDotUnsigned(surfaceNormal, halfDir);
							}
							else
							{
								specularComponent = XMVectorDotUnsigned(reflectedLight, cameraDir);
							}
							specularComponent = XMVectorPow(specularComponent, shininess);
							specularComponent = XMColorModulate(specularComponent, materialSpecular);
							specularComponent = XMColorModulate(specularComponent, l.specular);
							totalIntensity = XMVectorAdd(totalIntensity, specularComponent);
						}
					}
					XMFLOAT3 output;
					XMStoreFloat3(&output, totalIntensity);

					render_target->item(x, y) = unsigned_color::from_color(color::from_XMFLOAT3(output));
				}
				else
				{
					// miss shader
					// WARNING: RENDERING BOT SKY GRID AND FLOOR GRID IS NOT RECOMMENDED
					// THEY OVERLAP EACH OTHER AND LOOK UGLY:)

					if (render_axes(x, y, eye, cameraRay))
					{
						continue;
					}
					if (render_floor_grid(x, y, eye, cameraRay))
					{
						continue;
					}
					//if (render_sky_grid(x, y, eye, cameraRay))
					//{
					//	continue;
					//}
				}
			}
		}
	}

	template <typename VB, typename RT>
	inline bool raytracer<VB, RT>::trace_ray(
		const ray& ray, float max_t, float min_t, payload& outPayload, const bool bIsShadowRay) const
	{
		using namespace DirectX;
		std::set<payload> hits;

		for (size_t modelIdx = 0; modelIdx != index_buffers.size(); ++modelIdx)
		{
			const size_t numIndices = index_buffers.at(modelIdx)->get_number_of_elements();
			const size_t numFaces = numIndices / 3;

			for (size_t faceIdx = 0; faceIdx != numFaces; ++faceIdx)
			{
				// Extract triangle
				std::array<vertex, 3> face;
				std::array<XMVECTOR, 3> triangle;
				for (size_t i = 0; i != 3; ++i)
				{
					const unsigned index = index_buffers.at(modelIdx)->item(3 * faceIdx + i);
					face.at(i) = vertex_buffers.at(modelIdx)->item(index);
					triangle.at(i) = XMLoadFloat3(&face.at(i).position);
				}

				const XMVECTOR faceBasisX = XMVectorSubtract(triangle.at(1), triangle.at(0));
				const XMVECTOR faceBasisY = XMVectorSubtract(triangle.at(2), triangle.at(0));
				const XMVECTOR normal = XMVector3Normalize(XMVector3Cross(faceBasisY, faceBasisX));

				float t;
				if (TriangleTests::Intersects(
					ray.position, ray.direction, triangle.at(0), triangle.at(1), triangle.at(2), t))
				{
					if (t >= min_t && t <= max_t)
					{
						if (bIsShadowRay)
						{
							outPayload.depth = t;
							return true;
						}
						const XMVECTOR hitPoint = XMVectorAdd(ray.position, XMVectorScale(ray.direction, t));

						const XMVECTOR barycentric = XMFindBarycentric(hitPoint, triangle.at(0), triangle.at(1), triangle.at(2));

						assert(std::abs(XMVectorGetX(XMVectorSum(barycentric)) - 1.0f) < 0.001f);

						payload hit;
						hit.depth = t;
						hit.point = face.at(0) * XMVectorGetX(barycentric)
							+ face.at(1) * XMVectorGetY(barycentric)
							+ face.at(2) * XMVectorGetZ(barycentric);

						XMStoreFloat3(&hit.point.normal, normal);

						hits.insert(hit);
					}
				}
			}
		}

		if (!hits.empty())
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
	bool raytracer<VB, RT>::render_floor_grid(size_t x, size_t y, DirectX::FXMVECTOR eye, DirectX::FXMVECTOR dir)
	{
		using namespace DirectX;
		constexpr float thickness = 0.002f;
		constexpr float gridRadius = 8.0f; // in units
		if (!IsEqual(XMVectorGetY(dir), 0.0f))
		{
			const float t = -XMVectorGetY(eye) / XMVectorGetY(dir);
			if (t >= 0.0f)
			{
				const XMVECTOR floorHit = XMVectorAdd(eye, XMVectorScale(dir, t));
				const float floorX = XMVectorGetX(floorHit);
				const float floorZ = XMVectorGetZ(floorHit);
				if (floorX >= -gridRadius && floorX <= gridRadius)
				{
					if (floorZ >= -gridRadius && floorZ <= gridRadius)
					{
						if (IsEqual(floorX, std::round(floorX), thickness * t) ||
							IsEqual(floorZ, std::round(floorZ), thickness * t))
						{
							render_target->item(x, y) = unsigned_color::from_float3(float3(0.48f, 0.48f, 0.48f));
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	template <typename VB, typename RT>
	bool raytracer<VB, RT>::render_axes(size_t x, size_t y, DirectX::FXMVECTOR eye, DirectX::FXMVECTOR dir)
	{
		using namespace DirectX;
		constexpr float thickness = 0.003f;
		if (!IsEqual(XMVectorGetY(dir), 0.0f)) // X-axis, red
		{
			const float t = -XMVectorGetY(eye) / XMVectorGetY(dir);
			if (t >= 0.0f)
			{
				const XMVECTOR planeXZ = XMVectorAdd(eye, XMVectorScale(dir, t));
				const float axisZ = XMVectorGetZ(planeXZ);
				if (IsEqual(axisZ, 0.0f, thickness * t))
				{
					render_target->item(x, y) = unsigned_color::from_float3(float3(0.96f, 0.21f, 0.32f));
					return true;
				}
			}
		}
		if (!IsEqual(XMVectorGetZ(dir), 0.0f)) // Y-axis, green
		{
			const float t = -XMVectorGetZ(eye) / XMVectorGetZ(dir);
			if (t >= 0.0f)
			{
				const XMVECTOR planeXY = XMVectorAdd(eye, XMVectorScale(dir, t));
				const float axisX = XMVectorGetX(planeXY);
				if (IsEqual(axisX, 0.0f, thickness * t))
				{
					render_target->item(x, y) = unsigned_color::from_float3(float3(0.54f, 0.79f, 0.13f));
					return true;
				}
			}
		}
		if (!IsEqual(XMVectorGetX(dir), 0.0f)) // Z-axis, blue
		{
			const float t = -XMVectorGetX(eye) / XMVectorGetX(dir);
			if (t >= 0.0f)
			{
				const XMVECTOR planeYZ = XMVectorAdd(eye, XMVectorScale(dir, t));
				const float axisY = XMVectorGetY(planeYZ);
				if (IsEqual(axisY, 0.0f, thickness * t))
				{
					render_target->item(x, y) = unsigned_color::from_float3(float3(0.18f, 0.52f, 0.89f));
					return true;
				}
			}
		}
		return false;
	}

	template<typename VB, typename RT>
	bool raytracer<VB, RT>::render_sky_grid(size_t x, size_t y, DirectX::FXMVECTOR eye, DirectX::FXMVECTOR dir)
	{
		using namespace DirectX;
		constexpr float thickness = 0.0075f;
		constexpr float majorDensity = 10.0f; // every 10 degrees
		constexpr float minorSubdiv = 5.0f; // every 10 / 5 =  every 2 degrees

		if (!IsEqual(XMVectorGetY(dir), 1.0f))
		{
			float phi = XMConvertToDegrees(std::acos(XMVectorGetY(dir)));
			float theta = XMConvertToDegrees(std::atan(XMVectorGetX(dir) / XMVectorGetZ(dir))) + 90.0f;

			phi /= majorDensity;
			theta /= majorDensity;

			if (IsEqual(phi, std::round(phi), thickness) || IsEqual(theta, std::round(theta), thickness))
			{
				render_target->item(x, y) = unsigned_color::from_float3(float3(0.78f, 0.78f, 0.78f));
				return true;
			}

			phi *= minorSubdiv;
			theta *= minorSubdiv;

			if (IsEqual(phi, std::round(phi), thickness * 2) || IsEqual(theta, std::round(theta), thickness * 2))
			{
				render_target->item(x, y) = unsigned_color::from_float3(float3(0.6f, 0.6f, 0.6f));
				return true;
			}
		}
		return false;
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
