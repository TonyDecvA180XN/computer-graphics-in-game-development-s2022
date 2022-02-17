#pragma once

#include "resource.h"

#include <cmath>
#include <linalg.h>
#include <memory>
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

		std::vector<DirectX::BoundingBox> acceleration_structures;

		void ray_generation(size_t frame_id);

		bool trace_ray(const ray& ray, float max_t, float min_t, payload& payload, bool bIsShadowRay = false) const;

		DirectX::XMVECTOR hit_shader(const payload& p, const ray& camera_ray);

		DirectX::XMVECTOR miss_shader(const payload& p, const ray& camera_ray);

		bool render_floor_grid(const ray& camera_ray, DirectX::XMVECTOR& output);
		bool render_axes(const ray& camera_ray, DirectX::XMVECTOR& output);
		bool render_sky_grid(const ray& camera_ray, DirectX::XMVECTOR& output);

		DirectX::XMFLOAT2 get_jitter(size_t frame_id);

	protected:
		std::shared_ptr<resource<RT>> render_target;
		std::shared_ptr<resource<RT>> history;
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
		history = std::make_shared<resource<RT>>(width, height);
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
		using namespace DirectX;
		acceleration_structures.clear();
		acceleration_structures.reserve(vertex_buffers.size());

		for (std::shared_ptr<resource<VB>>& vb : vertex_buffers)
		{
			BoundingBox aabb;
			BoundingBox::CreateFromPoints(aabb, vb->get_number_of_elements(), &vb->item(0).position, sizeof(VB));
			acceleration_structures.emplace_back(aabb);
		}
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
	inline void raytracer<VB, RT>::ray_generation(size_t frame_id)
	{
		using namespace DirectX;

		const float h = static_cast<float>(height);
		const float w = static_cast<float>(width);
		const float minZ = camera->get_z_near();
		const float maxZ = camera->get_z_far();
		const XMVECTOR eye = camera->get_position();
		const XMMATRIX world = XMMatrixIdentity();
		const XMMATRIX view = camera->get_view_matrix();
		XMMATRIX projection = camera->get_projection_matrix();

		XMFLOAT2 jitter = get_jitter(frame_id);
		jitter.x = (jitter.x * 2.0f - 1.0f) / w * 2;
		jitter.y = (jitter.y * 2.0f - 1.0f) / h * 2;
		projection.r[2] = XMVectorAdd(projection.r[2], XMLoadFloat2(&jitter));

		for (size_t y = 0; y != height; ++y)
		{
			for (size_t x = 0; x != width; ++x)
			{
				const float fx = static_cast<float>(x);
				const float fy = static_cast<float>(y);
				const XMVECTOR pixel = XMVectorSet(fx, fy, 1.0f, 0.0f);
				XMVECTOR pixelDir = XMVector3Normalize(XMVector3Unproject(pixel,
																		  0.0f, 0.0f, w, h,
																		  0.0f, 1.0f,
																		  projection, view, world));
				ray r(eye, pixelDir);

				payload p;
				if (trace_ray(r, maxZ, minZ, p))
				{
					const XMVECTOR output = hit_shader(p, r);
					render_target->item(x, y) = unsigned_color::from_xmvector(output);
				}
				else
				{
					const XMVECTOR output = miss_shader(p, r);
					if (XMVectorGetX(XMVector3Length(output)) > 0)
					{
						render_target->item(x, y) = unsigned_color::from_xmvector(output);
					}
				}

				XMVECTOR current_color = render_target->item(x, y).to_xmvector();
				const XMVECTOR history_color = history->item(x, y).to_xmvector();
				if (frame_id > 0) // skip 1st frame
				{
					const float mix_factor = 0.75f;
					current_color = XMVectorLerp(current_color, history_color, mix_factor);
				}
				render_target->item(x, y) = unsigned_color::from_xmvector(current_color);
				history->item(x, y) = unsigned_color::from_xmvector(current_color);
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
			float _;
			if (!acceleration_structures[modelIdx].Intersects(ray.position, ray.direction, _))
			{
				continue;
			}

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

	template<typename VB, typename RT>
	DirectX::XMVECTOR raytracer<VB, RT>::hit_shader(const payload& p, const ray& camera_ray)
	{
		using namespace DirectX;

		// Use this switchers to play with parameters
		constexpr bool USE_BLINN_LIGHTING = false;
		constexpr bool USE_AMBIENT = true;
		constexpr bool USE_DIFFUSE = true;
		constexpr bool USE_SPECULAR = true;

		std::vector<light> lights;
		lights.push_back({
			XMVectorSet(0.0f, 1.925f, 0.0f, 1.0f),
			XMVectorSet(0.25f, 0.25f, 0.25f, 1.0f),
			XMVectorSet(0.75f, 0.75f, 0.75f, 1.0f),
			XMVectorSet(0.4f, 0.4f, 0.4f, 1.0f)
		});


		XMVECTOR output = XMVectorZero();
		for (const light& l : lights)
		{
			const XMVECTOR address = XMLoadFloat3(&p.point.position);
			const XMVECTOR surfaceNormal = XMLoadFloat3(&p.point.normal);
			const XMVECTOR lightVector = XMVectorSubtract(l.position, address);
			const XMVECTOR lightDir = XMVector3Normalize(lightVector);
			const XMVECTOR incident = XMVectorScale(lightDir, -1.0f);
			const XMVECTOR reflectedLight = XMVector3Reflect(incident, surfaceNormal);
			const XMVECTOR cameraDir = XMVector3Normalize(XMVectorSubtract(camera_ray.position, address));
			XMVECTOR shininess = XMVectorReplicate(p.point.shininess);
			XMVECTOR shadow = XMVectorSplatOne();

			if (USE_AMBIENT)
			{
				// add ambient component
				const XMVECTOR materialAmbient = XMLoadFloat3(&p.point.ambient);
				const XMVECTOR ambientComponent = XMColorModulate(l.ambient, materialAmbient);
				output = XMVectorAdd(output, ambientComponent);
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
				const XMVECTOR materialDiffuse = XMLoadFloat3(&p.point.diffuse);
				XMVECTOR diffuseComponent = XMVectorDotUnsigned(lightDir, surfaceNormal);
				diffuseComponent = XMColorModulate(diffuseComponent, l.duffuse);
				diffuseComponent = XMColorModulate(diffuseComponent, shadow);
				diffuseComponent = XMColorModulate(diffuseComponent, materialDiffuse);

				output = XMVectorAdd(output, diffuseComponent);
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
				output = XMVectorAdd(output, specularComponent);
			}
		}
		return output;
	}

	template<typename VB, typename RT>
	DirectX::XMVECTOR raytracer<VB, RT>::miss_shader(const payload& p, const ray& camera_ray) 
	{
		// miss shader
		// WARNING: RENDERING BOT SKY GRID AND FLOOR GRID IS NOT RECOMMENDED
		// THEY OVERLAP EACH OTHER AND LOOK UGLY:)
		//
		// ALSO: GRID AND AXES ARE NOT AFFECTED BY TAA BECAUSE THEY ARE
		// RENDERED MATHEMATICALLY

		DirectX::XMVECTOR output = DirectX::XMVectorZero();
		if (render_axes(camera_ray, output))
		{
			return output;
		}
		if (render_floor_grid(camera_ray, output))
		{
			return output;
		}
		//if (render_sky_grid(camera_ray, output))
		//{
		//	return output;
		//}
		return output;
	}

	template <typename VB, typename RT>
	bool raytracer<VB, RT>::render_floor_grid(const ray& camera_ray, DirectX::XMVECTOR& output)
	{
		using namespace DirectX;
		constexpr float thickness = 0.002f;
		constexpr float gridRadius = 8.0f; // in units
		if (!IsEqual(XMVectorGetY(camera_ray.direction), 0.0f))
		{
			const float t = -XMVectorGetY(camera_ray.position) / XMVectorGetY(camera_ray.direction);
			if (t >= 0.0f)
			{
				const XMVECTOR floorHit = XMVectorAdd(camera_ray.position, XMVectorScale(camera_ray.direction, t));
				const float floorX = XMVectorGetX(floorHit);
				const float floorZ = XMVectorGetZ(floorHit);
				if (floorX >= -gridRadius && floorX <= gridRadius)
				{
					if (floorZ >= -gridRadius && floorZ <= gridRadius)
					{
						if (IsEqual(floorX, std::round(floorX), thickness * t) ||
							IsEqual(floorZ, std::round(floorZ), thickness * t))
						{
							output = XMVectorSet(0.48f, 0.48f, 0.48f, 1.0f);
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	template <typename VB, typename RT>
	bool raytracer<VB, RT>::render_axes(const ray& camera_ray, DirectX::XMVECTOR& output)
	{
		using namespace DirectX;
		constexpr float thickness = 0.003f;
		if (!IsEqual(XMVectorGetY(camera_ray.direction), 0.0f)) // X-axis, red
		{
			const float t = -XMVectorGetY(camera_ray.position) / XMVectorGetY(camera_ray.direction);
			if (t >= 0.0f)
			{
				const XMVECTOR planeXZ = XMVectorAdd(camera_ray.position, XMVectorScale(camera_ray.direction, t));
				const float axisZ = XMVectorGetZ(planeXZ);
				if (IsEqual(axisZ, 0.0f, thickness * t))
				{
					output = XMVectorSet(0.96f, 0.21f, 0.32f, 1.0f);
					return true;
				}
			}
		}
		if (!IsEqual(XMVectorGetZ(camera_ray.direction), 0.0f)) // Y-axis, green
		{
			const float t = -XMVectorGetZ(camera_ray.position) / XMVectorGetZ(camera_ray.direction);
			if (t >= 0.0f)
			{
				const XMVECTOR planeXY = XMVectorAdd(camera_ray.position, XMVectorScale(camera_ray.direction, t));
				const float axisX = XMVectorGetX(planeXY);
				if (IsEqual(axisX, 0.0f, thickness * t))
				{
					output = XMVectorSet(0.54f, 0.79f, 0.13f, 1.0f);
					return true;
				}
			}
		}
		if (!IsEqual(XMVectorGetX(camera_ray.direction), 0.0f)) // Z-axis, blue
		{
			const float t = -XMVectorGetX(camera_ray.position) / XMVectorGetX(camera_ray.direction);
			if (t >= 0.0f)
			{
				const XMVECTOR planeYZ = XMVectorAdd(camera_ray.position, XMVectorScale(camera_ray.direction, t));
				const float axisY = XMVectorGetY(planeYZ);
				if (IsEqual(axisY, 0.0f, thickness * t))
				{
					output = XMVectorSet(0.18f, 0.52f, 0.89f, 1.0f);
					return true;
				}
			}
		}
		return false;
	}

	template<typename VB, typename RT>
	bool raytracer<VB, RT>::render_sky_grid(const ray& camera_ray, DirectX::XMVECTOR& output)
	{
		using namespace DirectX;
		constexpr float thickness = 0.0075f;
		constexpr float majorDensity = 10.0f; // every 10 degrees
		constexpr float minorSubdiv = 5.0f; // every 10 / 5 =  every 2 degrees

		if (!IsEqual(XMVectorGetY(camera_ray.direction), 1.0f))
		{
			float phi = XMConvertToDegrees(std::acos(XMVectorGetY(camera_ray.direction)));
			float theta = XMConvertToDegrees(std::atan(XMVectorGetX(camera_ray.direction) / XMVectorGetZ(camera_ray.direction))) + 90.0f;

			phi /= majorDensity;
			theta /= majorDensity;

			if (IsEqual(phi, std::round(phi), thickness) || IsEqual(theta, std::round(theta), thickness))
			{
				output = XMVectorSet(0.78f, 0.78f, 0.78f, 1.0f);
				return true;
			}

			phi *= minorSubdiv;
			theta *= minorSubdiv;

			if (IsEqual(phi, std::round(phi), thickness * 2) || IsEqual(theta, std::round(theta), thickness * 2))
			{
				output = XMVectorSet(0.6f, 0.6f, 0.6f, 1.0f);
				return true;
			}
		}
		return false;
	}

	template <typename VB, typename RT>
	DirectX::XMFLOAT2 raytracer<VB, RT>::get_jitter(size_t frame_id)
	{
		DirectX::XMFLOAT2 result(0.0f, 0.0f);
		constexpr int base_x = 2;
		int index = static_cast<int>(frame_id) + 1;
		float inv_base = 1.0f / base_x;
		float fraction = inv_base;
		while (index > 0)
		{
			result.x += (index % base_x) * fraction;
			index /= base_x;
			fraction *= inv_base;
		}
		constexpr int base_y = 3;
		index = static_cast<int>(frame_id) + 1;
		inv_base = 1.0f / base_y;
		fraction = inv_base;
		while (index > 0)
		{
			result.y += index % base_y * fraction;
			index /= base_y;
			fraction *= inv_base;
		}
		return result;
	}
} // namespace cg::renderer
