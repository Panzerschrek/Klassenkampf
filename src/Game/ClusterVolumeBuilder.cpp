#include "ClusterVolumeBuilder.hpp"
#include "Assert.hpp"


namespace KK
{


ClusterVolumeBuilder::ClusterVolumeBuilder(
	const uint32_t width,
	const uint32_t height,
	const uint32_t depth)
	: size_{ width, height, depth }
	, clusters_(width * height * depth)
{}

void ClusterVolumeBuilder::SetMatrix(const m_Mat4& mat, const float m10, const float m14)
{
	matrix_= mat;
	m10_= m10;
	m14_= m14;
}

void ClusterVolumeBuilder::AddSphere(const m_Vec3& center, const float radius, const ElementId id)
{
	// Createworld space bounding box.
	const m_Vec3 box_corners[8]
	{
		m_Vec3(center.x + radius, center.y + radius, center.z + radius),
		m_Vec3(center.x + radius, center.y + radius, center.z - radius),
		m_Vec3(center.x + radius, center.y - radius, center.z + radius),
		m_Vec3(center.x + radius, center.y - radius, center.z - radius),
		m_Vec3(center.x - radius, center.y + radius, center.z + radius),
		m_Vec3(center.x - radius, center.y + radius, center.z - radius),
		m_Vec3(center.x - radius, center.y - radius, center.z + radius),
		m_Vec3(center.x - radius, center.y - radius, center.z - radius),
	};

	// Project it.
	m_Vec3 box_corners_projected[8];
	float box_corners_projected_w[8];
	float box_corners_depth[8];
	for(size_t i= 0u; i < 8u; ++i)
	{
		box_corners_projected[i]= box_corners[i] * matrix_;
		box_corners_projected_w[i]= box_corners[i].x + matrix_.value[3] + box_corners[i].y * matrix_.value[7] + box_corners[i].z + matrix_.value[11] + matrix_.value[15];
		box_corners_depth[i]= box_corners_projected[i].z / box_corners_projected_w[i];
	}

	// Calculate view space min/max.
	m_Vec3 bb_min= box_corners_projected[0];
	m_Vec3 bb_max= box_corners_projected[0];
	float depth_min= box_corners_depth[0];
	float depth_max= box_corners_depth[0];
	for(size_t i= 1u; i < 8u; ++i)
	{
		bb_min.x= std::min(bb_min.x, box_corners_projected[i].x);
		bb_min.y= std::min(bb_min.y, box_corners_projected[i].y);
		bb_min.z= std::min(bb_min.x, box_corners_projected[i].z);
		bb_max.x= std::max(bb_max.x, box_corners_projected[i].x);
		bb_max.y= std::max(bb_max.y, box_corners_projected[i].y);
		bb_max.z= std::max(bb_max.x, box_corners_projected[i].z);
		depth_min= std::min(depth_min, box_corners_depth[i]);
		depth_max= std::max(depth_max, box_corners_depth[i]);
	}

	depth_min= std::max(depth_min, 0.0f);
	depth_max= std::min(depth_max, 0.9999f);

	const uint32_t slice_min= uint32_t(float(size_[2]) * depth_min);
	const uint32_t slice_max= uint32_t(float(size_[2]) * depth_max);
	for(uint32_t slice= slice_min; slice <= slice_max; ++slice)
	{
		const float slice_depth= float(slice) / float(size_[2]);
		const float slice_w= m14_ / (slice_depth - m10_);

		const float slice_min_x= std::max(bb_min.x / slice_w, -0.999f);
		const float slice_max_x= std::max(bb_max.x / slice_w, -0.999f);
		const float slice_min_y= std::min(bb_min.y / slice_w, +0.999f);
		const float slice_max_y= std::min(bb_max.y / slice_w, +0.999f);

		const uint32_t cluster_min_x= uint32_t((slice_min_x * 0.5f + 0.5f) * float(size_[0]));
		const uint32_t cluster_max_x= uint32_t((slice_max_x * 0.5f + 0.5f) * float(size_[0]));
		const uint32_t cluster_min_y= uint32_t((slice_min_y * 0.5f + 0.5f) * float(size_[1]));
		const uint32_t cluster_max_y= uint32_t((slice_max_y * 0.5f + 0.5f) * float(size_[1]));
		KK_ASSERT(cluster_min_x <= cluster_max_x);
		KK_ASSERT(cluster_min_y <= cluster_max_y);
		KK_ASSERT(cluster_max_x < size_[0]);
		KK_ASSERT(cluster_max_y < size_[1]);

		for(uint32_t x= cluster_min_x; x <= cluster_max_x; ++x)
		for(uint32_t y= cluster_min_y; y <= cluster_max_y; ++y)
			clusters_[x + y * size_[0] + slice * (size_[0] * size_[1])].elements.push_back(id);
	}
}

} // namespace KK
