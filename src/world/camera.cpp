#define _USE_MATH_DEFINES

#include "camera.h"

#include "utils/error_handler.h"

#include "DirectXMath.h"

#include <algorithm>

using namespace cg::world;

cg::world::camera::camera() : theta(0.f), phi(0.f), height(1080.f), width(1920.f),
							  aspect_ratio(1920.f / 1080.f), angle_of_view(1.04719f),
							  z_near(0.001f), z_far(100.f), position(float3{0.f, 0.f, 0.f})
{
}

cg::world::camera::~camera() {}

void cg::world::camera::set_position(float3 in_position)
{
	//THROW_ERROR("Not implemented yet");
	position = in_position;
}

void cg::world::camera::set_theta(float in_theta)
{
	//THROW_ERROR("Not implemented yet");
	theta = std::clamp(in_theta, -180.0f, 180.0f);
}

void cg::world::camera::set_phi(float in_phi)
{
	//THROW_ERROR("Not implemented yet");
	phi = std::clamp(in_phi, -90.0f, 90.0f);
}

void cg::world::camera::set_angle_of_view(float in_aov)
{
	//THROW_ERROR("Not implemented yet");
	angle_of_view = DirectX::XMConvertToRadians(in_aov);
}

void cg::world::camera::set_height(float in_height)
{
	//THROW_ERROR("Not implemented yet");
	height = in_height;
}

void cg::world::camera::set_width(float in_width)
{
	//THROW_ERROR("Not implemented yet");
	width = in_width;
}

void cg::world::camera::set_z_near(float in_z_near)
{
	//THROW_ERROR("Not implemented yet");
	z_near = in_z_near;
}

void cg::world::camera::set_z_far(float in_z_far)
{
	//THROW_ERROR("Not implemented yet");
	z_far = in_z_far;
}

const DirectX::XMMATRIX cg::world::camera::get_view_matrix() const
{
	//THROW_ERROR("Not implemented yet");

	return DirectX::XMMatrixLookToLH(get_position(), get_direction(), DirectX::XMVectorSet(0, 1, 0, 0));
}

#ifdef DX12
const DirectX::XMMATRIX cg::world::camera::get_dxm_view_matrix() const
{
	THROW_ERROR("Not implemented yet");
	return DirectX::XMMatrixIdentity();
}

const DirectX::XMMATRIX cg::world::camera::get_dxm_projection_matrix() const
{
	THROW_ERROR("Not implemented yet");
	return DirectX::XMMatrixIdentity();
}
#endif

const DirectX::XMMATRIX cg::world::camera::get_projection_matrix() const
{
	//THROW_ERROR("Not implemented yet");
	const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(angle_of_view, width / height, z_near, z_far);
	return projection;
}

const DirectX::XMVECTOR cg::world::camera::get_position() const
{
	//THROW_ERROR("Not implemented yet");
	return DirectX::XMVectorSet(position.x, position.y, position.z, 1.0);
}

const DirectX::XMVECTOR cg::world::camera::get_direction() const
{
	//THROW_ERROR("Not implemented yet");
	const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(DirectX::XMConvertToRadians(theta), DirectX::XMConvertToRadians(phi), 0.0f);
	const DirectX::XMVECTOR base = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	const DirectX::XMVECTOR look = DirectX::XMVector3Transform(base, rotation);
	return look;
}

const DirectX::XMVECTOR cg::world::camera::get_right() const
{
	//THROW_ERROR("Not implemented yet");
	const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	const DirectX::XMVECTOR look = get_direction();
	const DirectX::XMVECTOR right = DirectX::XMVector3Cross(look, up);

	return right;
}

const DirectX::XMVECTOR cg::world::camera::get_up() const
{
	//THROW_ERROR("Not implemented yet");

	const DirectX::XMVECTOR look = get_direction();
	const DirectX::XMVECTOR right = get_right();
	const DirectX::XMVECTOR up = DirectX::XMVector3Cross(look, right);

	return up;
}
const float camera::get_theta() const
{
	//THROW_ERROR("Not implemented yet");
	return theta;
}
const float camera::get_phi() const
{
	//THROW_ERROR("Not implemented yet");
	return phi;
}

const float camera::get_z_near() const
{
	return z_near;
}

const float camera::get_z_far() const
{
	return z_far;
}
