#include "WorldGenerator.hpp"
#include "Rand.hpp"


namespace KK
{

namespace
{

class WorldGenerator final
{
public:

	WorldData::World Generate()
	{
		{
			WorldData::Segment root_segment;
			root_segment.type=WorldData::SegmentType::Room;

			root_segment.bb_min[0]= -4;
			root_segment.bb_min[1]= -4;
			root_segment.bb_min[2]= +0;
			root_segment.bb_max[0]= +4;
			root_segment.bb_max[1]= +4;
			root_segment.bb_max[2]= +3;

			result_.segments.push_back(std::move(root_segment));
		}
		Genrate_r(0u);

		return std::move(result_);
	}

private:
	void Genrate_r(const size_t current_segment_index)
	{
		if(result_.segments.size() >= 64u)
			return;

		switch(result_.segments[current_segment_index].type)
		{
		case WorldData::SegmentType::Corridor:
			ProcessCorridor(current_segment_index);
			break;

		case WorldData::SegmentType::Room:
			ProcessRoom(current_segment_index);
			break;
		}
	}

	void ProcessRoom(const size_t segment_index)
	{
		(void)segment_index;
	}

	void ProcessCorridor(const size_t segment_index)
	{
		if(rand_.RandBool(1u, 5u))
			return;

		WorldData::CoordType room_start_coord[3];
		room_start_coord[2]= result_.segments[segment_index].bb_min[2];

		switch(result_.segments[segment_index].direction)
		{
		case WorldData::Direction::XPlus:
			room_start_coord[0]= result_.segments[segment_index].bb_max[0];
			room_start_coord[1]= result_.segments[segment_index].bb_max[1];
			break;
		case WorldData::Direction::XMinus:
			room_start_coord[0]= result_.segments[segment_index].bb_min[0];
			room_start_coord[1]= result_.segments[segment_index].bb_max[1];
			break;
		case WorldData::Direction::YPlus:
			room_start_coord[0]= result_.segments[segment_index].bb_min[0];
			room_start_coord[1]= result_.segments[segment_index].bb_max[1];
			break;
		case WorldData::Direction::YMinus:
			room_start_coord[0]= result_.segments[segment_index].bb_min[0];
			room_start_coord[1]= result_.segments[segment_index].bb_min[1];
			break;
		};

		WorldData::Segment new_room;
		new_room.type=WorldData::SegmentType::Room;
		new_room.bb_min[2]= room_start_coord[2];
		new_room.bb_max[2]= room_start_coord[2] + 2;
		// TODO
	}

	bool CanPlace(const WorldData::CoordType* const bb_min, const WorldData::CoordType* const bb_max)
	{
		for(const WorldData::Segment& segment : result_.segments)
		{
			if( segment.bb_min[0] > bb_max[0] ||
				segment.bb_min[1] > bb_max[1] ||
				segment.bb_min[2] > bb_max[2] ||
				segment.bb_max[0] < bb_min[0] ||
				segment.bb_max[1] < bb_min[1] ||
				segment.bb_max[2] < bb_min[2])
				continue;
			return false;
		}

		return true;
	}

private:
	WorldData::World result_;
	LongRand rand_;
};


} // namespace

WorldData::World GenerateWorld()
{
	WorldGenerator generator;
	return generator.Generate();
}

} // namespace KK
