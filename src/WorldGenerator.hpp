#pragma once
#include <vector>
#include <cstdint>


namespace KK
{

namespace WorldData
{

using CoordType= int32_t;

enum class SegmentType
{
	Room,
	Corridor,
};

enum class Direction
{
	XPlus,
	XMinus,
	YPlus,
	YMinus,
};

struct Segment
{
	SegmentType type= SegmentType::Room;
	Direction direction= Direction::XPlus;
	CoordType bb_min[3]{};
	CoordType bb_max[3]{};
};

struct World
{
	std::vector<Segment> segments;
};

} // namespace WorldData


WorldData::World GenerateWorld();

} // namespace KK
