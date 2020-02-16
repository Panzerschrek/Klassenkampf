#include "ClusterVolumeBuilder.hpp"
#include "Assert.hpp"


namespace KK
{

namespace
{

const float c_pow_factor= 64.0f;

float DepthMappingFunction(const float x)
{
	return std::pow(x, c_pow_factor);
}

float DepthUnmappingFunction(const float x)
{
	return std::pow(x, 1.0f / c_pow_factor);
}

} // namespace

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

bool ClusterVolumeBuilder::AddSphere(const m_Vec3& center, const float radius, const ElementId id)
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
	float box_corners_depth[8];
	for(size_t i= 0u; i < 8u; ++i)
	{
		box_corners_projected[i]= box_corners[i] * matrix_;
		const float w=
			matrix_.value[ 3] * box_corners[i].x +
			matrix_.value[ 7] * box_corners[i].y +
			matrix_.value[11] * box_corners[i].z +
			matrix_.value[15];
		if(w > 0.0f)
			box_corners_depth[i]= box_corners_projected[i].z / w;
		else
			box_corners_depth[i]= 0.0f;
	}

	// Calculate view space min/max.
	m_Vec2 bb_min= box_corners_projected[0].xy();
	m_Vec2 bb_max= box_corners_projected[0].xy();
	float depth_min= box_corners_depth[0];
	float depth_max= box_corners_depth[0];
	for(size_t i= 1u; i < 8u; ++i)
	{
		bb_min.x= std::min(bb_min.x, box_corners_projected[i].x);
		bb_min.y= std::min(bb_min.y, box_corners_projected[i].y);
		bb_max.x= std::max(bb_max.x, box_corners_projected[i].x);
		bb_max.y= std::max(bb_max.y, box_corners_projected[i].y);
		depth_min= std::min(depth_min, box_corners_depth[i]);
		depth_max= std::max(depth_max, box_corners_depth[i]);
	}

	depth_min= std::max(depth_min, 0.0f);
	depth_max= std::min(depth_max, 0.9999f);
	if(depth_min > 1.0f || depth_max < 0.0f)
		return false;

	const float depth_min_mapped= DepthMappingFunction(depth_min);
	const float depth_max_mapped= DepthMappingFunction(depth_max);

	bool added= false;
	const int32_t slice_min= int32_t(float(size_[2]) * depth_min_mapped);
	const int32_t slice_max= int32_t(float(size_[2]) * depth_max_mapped);
	for(int32_t slice= slice_min; slice <= slice_max; ++slice)
	{
		const float slice_depth_min= DepthUnmappingFunction(float(slice    ) / float(size_[2]));
		const float slice_depth_max= DepthUnmappingFunction(float(slice + 1) / float(size_[2]));
		const float slice_w_min= m14_ / (slice_depth_min - m10_);
		const float slice_w_max= m14_ / (slice_depth_max - m10_);
		KK_ASSERT(slice_w_min > 0.0f);
		KK_ASSERT(slice_w_max > 0.0f);

		const float slice_min_x= std::max(std::min(bb_min.x / slice_w_min, bb_min.x / slice_w_max), -0.95f);
		const float slice_max_x= std::min(std::max(bb_max.x / slice_w_min, bb_max.x / slice_w_max), +0.95f);
		const float slice_min_y= std::max(std::min(bb_min.y / slice_w_min, bb_min.y / slice_w_max), -0.95f);
		const float slice_max_y= std::min(std::max(bb_max.y / slice_w_min, bb_max.y / slice_w_max), +0.95f);

		const int32_t cluster_min_x= int32_t((slice_min_x * 0.5f + 0.5f) * float(size_[0]));
		const int32_t cluster_max_x= int32_t((slice_max_x * 0.5f + 0.5f) * float(size_[0]));
		const int32_t cluster_min_y= int32_t((slice_min_y * 0.5f + 0.5f) * float(size_[1]));
		const int32_t cluster_max_y= int32_t((slice_max_y * 0.5f + 0.5f) * float(size_[1]));

		added= added | (cluster_min_x <= cluster_max_x && cluster_min_y <= cluster_max_y);
		for(int32_t x= cluster_min_x; x <= cluster_max_x; ++x)
		for(int32_t y= cluster_min_y; y <= cluster_max_y; ++y)
		{
			KK_ASSERT(x >= 0 && x < int32_t(size_[0]));
			KK_ASSERT(y >= 0 && y < int32_t(size_[1]));
			KK_ASSERT(slice >= 0 && slice < int32_t(size_[2]));

			clusters_[
				x +
				y * int32_t(size_[0]) +
				slice * int32_t(size_[0] * size_[1])].elements.push_back(id);
		}
	}
	return added;
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
