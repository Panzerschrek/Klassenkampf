#include "ClusterVolumeBuilder.hpp"
#include "Assert.hpp"


namespace KK
{

namespace
{

// If this changed, it must be changed in shader code too!
float WMappingFunction(const m_Vec2& w_convert_values, const float w)
{
	return w_convert_values.x * std::log2(w_convert_values.y / w);
}

float WUnmappingFunction(const m_Vec2& w_convert_values, const float slice_number)
{
	return w_convert_values.y / std::exp2(slice_number / w_convert_values.x);
}

} // namespace

ClusterVolumeBuilder::ClusterVolumeBuilder(
	const uint32_t width,
	const uint32_t height,
	const uint32_t depth)
	: size_{ width, height, depth }
	, clusters_(width * height * depth)
{}

void ClusterVolumeBuilder::SetMatrix(const m_Mat4& mat, const float z_near, const float z_far)
{
	matrix_= mat;

	/* Use logaritmical distribution of slices.
	slice_number= slice_count * log2(w / z_near) / log(z_far / z_near)
	slice_number= (slice_count / log2(z_far / z_near)) * log2(w / z_near)
	slice_number= (slice_count / log2(z_near / z_far)) * log2(z_near / w)
	slice_number= w_convert_values_.x * log2(w_convert_values_.y * w); // With regular W
	slice_number= w_convert_values_.x * log2(w_convert_values_.y * gl_FragCoord.w); // In shader
	*/
	w_convert_values_.x= float(size_[2]) / std::log2(z_near / z_far);
	w_convert_values_.y= z_near;
}

void ClusterVolumeBuilder::ClearClusters()
{
	for(Cluster& cluster : clusters_)
		cluster.elements.clear();
}

bool ClusterVolumeBuilder::AddSphere(const m_Vec3& center, const float radius, const ElementId id)
{
	// Create world space bounding box.
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

	// Project it and calculate bounding box.
	m_Vec2 bb_min(+1e24f, +1e24f);
	m_Vec2 bb_max(-1e24f, -1e24f);
	float w_min= +1e24f;
	float w_max= -1e24f;
	for(size_t i= 0u; i < 8u; ++i)
	{
		const m_Vec3 proj= box_corners[i] * matrix_;
		const float w=
			matrix_.value[ 3] * box_corners[i].x +
			matrix_.value[ 7] * box_corners[i].y +
			matrix_.value[11] * box_corners[i].z +
			matrix_.value[15];

		bb_min.x= std::min(bb_min.x, proj.x);
		bb_min.y= std::min(bb_min.y, proj.y);
		bb_max.x= std::max(bb_max.x, proj.x);
		bb_max.y= std::max(bb_max.y, proj.y);
		w_min= std::min(w_min, w);
		w_max= std::max(w_max, w);
	}

	// Clamp values - logarith function exists only for positive numbers.
	w_min= std::max(w_min, 0.0001f);
	w_max= std::max(w_max, 0.0001f);

	const int32_t slice_min= int32_t(WMappingFunction(w_convert_values_, w_min));
	const int32_t slice_max= int32_t(WMappingFunction(w_convert_values_, w_max));
	if(slice_max < 0 || slice_min >= int32_t(size_[2]))
		return false;

	bool added= false;
	for(int32_t slice= std::max(0, slice_min); slice <= std::min(slice_max, int32_t(size_[2])); ++slice)
	{
		const float slice_w_min= std::max(w_min, WUnmappingFunction(w_convert_values_, float(slice    )));
		const float slice_w_max= std::min(w_max, WUnmappingFunction(w_convert_values_, float(slice + 1)));
		KK_ASSERT(slice_w_min > 0.0f);
		KK_ASSERT(slice_w_max > 0.0f);

		const float slice_min_x= std::max(std::min(bb_min.x / slice_w_min, bb_min.x / slice_w_max), -0.99f);
		const float slice_max_x= std::min(std::max(bb_max.x / slice_w_min, bb_max.x / slice_w_max), +0.99f);
		const float slice_min_y= std::max(std::min(bb_min.y / slice_w_min, bb_min.y / slice_w_max), -0.99f);
		const float slice_max_y= std::min(std::max(bb_max.y / slice_w_min, bb_max.y / slice_w_max), +0.99f);

		const int32_t cluster_min_x= int32_t((slice_min_x * 0.5f + 0.5f) * float(size_[0]));
		const int32_t cluster_max_x= int32_t((slice_max_x * 0.5f + 0.5f) * float(size_[0]));
		const int32_t cluster_min_y= int32_t((slice_min_y * 0.5f + 0.5f) * float(size_[1]));
		const int32_t cluster_max_y= int32_t((slice_max_y * 0.5f + 0.5f) * float(size_[1]));

		added= added | (cluster_min_x <= cluster_max_x && cluster_min_y <= cluster_max_y);
		for(int32_t y= cluster_min_y; y <= cluster_max_y; ++y)
		for(int32_t x= cluster_min_x; x <= cluster_max_x; ++x)
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

m_Vec2 ClusterVolumeBuilder::GetWConvertValues() const
{
	return w_convert_values_;
}

} // namespace KK
