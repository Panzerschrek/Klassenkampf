#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>


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
	FloorWallWallJoint,
	Corridor,
	Shaft,
	CeilingArch3,
	Column3,
	Column3Lights,
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
	Joint,
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
	CoordType ceiling_height= 0;
	CoordType columns_step= 0;

	std::vector<Segment> segments;
};

struct Portal
{
	// bb_min = bb_max for one of components.
	CoordType bb_min[3]{};
	CoordType bb_max[3]{};
};

struct World
{
	std::vector<Sector> sectors;
	std::vector<Portal> portals;
};

} // namespace WorldData


WorldData::World GenerateWorld();

} // namespace KK
