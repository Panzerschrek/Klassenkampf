#pragma once
#include "../MathLib/Mat.hpp"
#include "Settings.hpp"
#include "SystemEvent.hpp"


namespace KK
{

class CameraController final
{
public:
	CameraController(Settings& settings, float aspect);

	void Update(float time_delta_s, const InputState& input_state);

	m_Mat4 CalculateViewMatrix() const;

private:
	Settings& settings_;
	const float aspect_;

	m_Vec3 pos_= m_Vec3(0.0f, 0.0f, 0.0f);
	float azimuth_= 0.0f;
	float elevation_= 0.0f;
};

} // namespace KK
