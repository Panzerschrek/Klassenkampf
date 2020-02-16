#pragma once
#include "../MathLib/Mat.hpp"
#include <cstdint>
#include <vector>


namespace KK
{

class ClusterVolumeBuilder final
{
public:
	using ElementId= uint8_t; // TODO - maybe use less bits?

	struct Cluster
	{
		// TODO - remove dynamic allocation, use array with constant size or linked list.
		std::vector<ElementId> elements;
	};

public:
	ClusterVolumeBuilder(uint32_t width, uint32_t height, uint32_t depth);

	void SetMatrix(const m_Mat4& mat, float m10, float m14);
	void ClearClusters();

	// Returns true, if added.
	bool AddSphere(const m_Vec3& center, float radius, ElementId id);

	uint32_t GetWidth () const;
	uint32_t GetHeight() const;
	uint32_t GetDepth () const;
	const std::vector<Cluster>& GetClusters() const;

private:
	const uint32_t size_[3];
	std::vector<Cluster> clusters_;
	m_Mat4 matrix_;
	float m10_, m14_; // Numbers of perspective matrix.
};

} // namespace KK
