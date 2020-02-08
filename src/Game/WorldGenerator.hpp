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
	Floor,
	Wall,
	FloorWallJoint,
	Corridor,
	Shaft,
};

struct Segment
{
	CoordType pos[3];
	SegmentType type;
	uint8_t angle; // 4 - 2 * pi
};

enum class SectorType
{
	Room,
	Corridor,
	Shaft,
};

enum class Direction
{
	XPlus,
	XMinus,
	YPlus,
	YMinus,
};

struct Sector
{
	SectorType type= SectorType::Room;
	Direction direction= Direction::XPlus;
	CoordType bb_min[3]{};
	CoordType bb_max[3]{};

	std::vector<Segment> segments;
};

struct World
{
	std::vector<Sector> sectors;
};

} // namespace WorldData


WorldData::World GenerateWorld();

} // namespace KK
