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

void ClusterVolumeBuilder::ClearClusters()
{
	for(Cluster& cluster : clusters_)
		cluster.elements.clear();
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

	const int32_t slice_min= int32_t(float(size_[2]) * depth_min);
	const int32_t slice_max= int32_t(float(size_[2]) * depth_max);
	for(int32_t slice= slice_min; slice <= slice_max; ++slice)
	{
		const float slice_depth= float(slice) / float(size_[2]);
		const float slice_w= m14_ / (slice_depth - m10_);

		const float slice_min_x= std::max(bb_min.x / slice_w, -0.95f);
		const float slice_max_x= std::min(bb_max.x / slice_w, +0.95f);
		const float slice_min_y= std::max(bb_min.y / slice_w, -0.95f);
		const float slice_max_y= std::min(bb_max.y / slice_w, +0.95f);

		const int32_t cluster_min_x= int32_t((slice_min_x * 0.5f + 0.5f) * float(size_[0]));
		const int32_t cluster_max_x= int32_t((slice_max_x * 0.5f + 0.5f) * float(size_[0]));
		const int32_t cluster_min_y= int32_t((slice_min_y * 0.5f + 0.5f) * float(size_[1]));
		const int32_t cluster_max_y= int32_t((slice_max_y * 0.5f + 0.5f) * float(size_[1]));

		for(int32_t x= cluster_min_x; x <= cluster_max_x; ++x)
		for(int32_t y= cluster_min_y; y <= cluster_max_y; ++y)
		{
			KK_ASSERT(x < int32_t(size_[0]));
			KK_ASSERT(y < int32_t(size_[1]));

			clusters_[x + y * int32_t(size_[0]) + slice * int32_t(size_[0] * size_[1])].elements.push_back(id);
		}
	}
}

uint32_t ClusterVolumeBuilder::GetWidth () const
{
	return size_[0];
}

uint32_t ClusterVolumeBuilder::GetHeight() const
{
	return size_[1];
}

uint32_t ClusterVolumeBuilder::GetDepth () const
{
	return size_[2];
}

const std::vector<ClusterVolumeBuilder::Cluster>& ClusterVolumeBuilder::GetClusters() const
{
	return clusters_;
}

} // namespace KK
