#include "CameraController.hpp"
#include "../MathLib/MathConstants.hpp"
#include <algorithm>


namespace KK
{

namespace
{

const float g_pi= 3.1415926535f;

} // namespace

CameraController::CameraController(Settings& settings, const float aspect)
	: settings_(settings), aspect_(std::move(aspect))
{}

void CameraController::Update(const float time_delta_s, const InputState& input_state)
{
	const float speed= std::max(0.125f, std::min(float(settings_.GetReal("cl_speed", 3.0f)), 32.0f));
	const float jump_speed= 0.8f * speed;
	const float angle_speed= std::max(0.125f, std::min(float(settings_.GetReal("cl_angle_speed", 1.0f)), 4.0f));

	settings_.SetReal("cl_speed", speed);
	settings_.SetReal("cl_angle_speed", angle_speed);

	const m_Vec3 forward_vector(-std::sin(azimuth_), +std::cos(azimuth_), 0.0f);
	const m_Vec3 left_vector(std::cos(azimuth_), std::sin(azimuth_), 0.0f);

	m_Vec3 move_vector(0.0f, 0.0f, 0.0f);

	using KeyCode= SystemEventTypes::KeyCode;
	if(input_state.keyboard[size_t(KeyCode::W)])
		move_vector+= forward_vector;
	if(input_state.keyboard[size_t(KeyCode::S)])
		move_vector-= forward_vector;
	if(input_state.keyboard[size_t(KeyCode::D)])
		move_vector+= left_vector;
	if(input_state.keyboard[size_t(KeyCode::A)])
		move_vector-= left_vector;

	const float move_vector_length= move_vector.GetLength();
	if(move_vector_length > 0.0f)
		pos_+= move_vector * (time_delta_s * speed / move_vector_length);

	if(input_state.keyboard[size_t(KeyCode::Space)])
		pos_.z+= time_delta_s * jump_speed;
	if(input_state.keyboard[size_t(KeyCode::C)])
		pos_.z-= time_delta_s * jump_speed;

	if(input_state.keyboard[size_t(KeyCode::Left)])
		azimuth_+= time_delta_s * angle_speed;
	if(input_state.keyboard[size_t(KeyCode::Right)])
		azimuth_-= time_delta_s * angle_speed;

	if(input_state.keyboard[size_t(KeyCode::Up)])
		elevation_+= time_delta_s * angle_speed;
	if(input_state.keyboard[size_t(KeyCode::Down)])
		elevation_-= time_delta_s * angle_speed;

	while(azimuth_ > +g_pi)
		azimuth_-= 2.0f * g_pi;
	while(azimuth_ < -g_pi)
		azimuth_+= 2.0f * g_pi;

	elevation_= std::max(-0.5f * g_pi, std::min(elevation_, +0.5f * g_pi));
}

CameraController::ViewMatrix CameraController::CalculateViewMatrix() const
{
	const float fov_deg= std::max(1.0f, std::min(float(settings_.GetReal("cl_fov", 90.0)), 135.0f));
	settings_.SetReal("cl_fov", fov_deg);

	const float fov= fov_deg * MathConstants::deg2rad;

	// Must be power of 2.
	const float z_near= 0.125f;
	const float z_far= 128.0f;

	m_Mat4 translate, rotate_z, rotate_x, basis_change, perspective;
	translate.Translate(-pos_);
	rotate_x.RotateX(-elevation_);
	rotate_z.RotateZ(-azimuth_);

	basis_change.MakeIdentity();
	basis_change.value[5]= 0.0f;
	basis_change.value[6]= 1.0f;
	basis_change.value[9]= -1.0f;
	basis_change.value[10]= 0.0f;

	perspective.PerspectiveProjection(aspect_, fov, z_near, z_far);

	ViewMatrix result;
	result.mat= translate * rotate_z * rotate_x * basis_change * perspective;

	result.z_near= z_near;
	result.z_far= z_far;

	result. m0= perspective.value[ 0];
	result. m5= perspective.value[ 5];
	result.m10= perspective.value[10];
	result.m14= perspective.value[14];

	return result;
}

m_Vec3 CameraController::GetCameraPosition() const
{
	return pos_;
}

} // namespace KK
