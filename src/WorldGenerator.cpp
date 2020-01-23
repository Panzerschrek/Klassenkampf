#include "WorldGenerator.hpp"
#include "Assert.hpp"
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
		const WorldData::Segment segment= result_.segments[segment_index];

		for(size_t dir_index= 0u; dir_index < 4u; ++dir_index)
		{
			const WorldData::Direction dir= WorldData::Direction(dir_index);

			if(rand_.RandBool(1u, 4u))
				continue;

			WorldData::Segment new_corridor;
			new_corridor.type= WorldData::SegmentType::Corridor;
			new_corridor.direction= dir;
			new_corridor.bb_min[2]= segment.bb_min[2];
			new_corridor.bb_max[2]= segment.bb_min[2] + 1;

			const WorldData::CoordType corridor_length= WorldData::CoordType(rand_.Rand() % 7u + 1u);

			switch(dir)
			{
			case WorldData::Direction::XPlus:
				new_corridor.bb_min[0]= segment.bb_max[0];
				new_corridor.bb_max[0]= segment.bb_max[0] + corridor_length;
				new_corridor.bb_min[1]= (segment.bb_min[1] + segment.bb_max[1]) >> 1;
				new_corridor.bb_max[1]= new_corridor.bb_min[1] + 1;
				break;

			case WorldData::Direction::XMinus:
				new_corridor.bb_max[0]= segment.bb_min[0];
				new_corridor.bb_min[0]= segment.bb_min[0] - corridor_length;
				new_corridor.bb_min[1]= (segment.bb_min[1] + segment.bb_max[1]) >> 1;
				new_corridor.bb_max[1]= new_corridor.bb_min[1] + 1;
				break;

			case WorldData::Direction::YPlus:
				new_corridor.bb_min[0]= (segment.bb_min[0] + segment.bb_max[0]) >> 1;
				new_corridor.bb_max[0]= new_corridor.bb_min[0] + 1;
				new_corridor.bb_min[1]= segment.bb_max[1];
				new_corridor.bb_max[1]= segment.bb_max[1] + corridor_length;
				break;

			case WorldData::Direction::YMinus:
				new_corridor.bb_min[0]= (segment.bb_min[0] + segment.bb_max[0]) >> 1;
				new_corridor.bb_max[0]= new_corridor.bb_min[0] + 1;
				new_corridor.bb_max[1]= segment.bb_min[1];
				new_corridor.bb_min[1]= segment.bb_min[1] - corridor_length;
				break;
			};

			for(size_t i= 0u; i < 3u; ++i)
				KK_ASSERT(new_corridor.bb_min[0] < new_corridor.bb_max[0]);

			if(!CanPlace(new_corridor.bb_min, new_corridor.bb_max))
				return;

			result_.segments.push_back(new_corridor);
			Genrate_r(result_.segments.size() - 1u);
		}
	}

	void ProcessCorridor(const size_t segment_index)
	{
		if(rand_.RandBool(1u, 5u))
			return;

		const WorldData::CoordType room_size[]=
		{
			WorldData::CoordType(rand_.Rand() % 5u + 3u),
			WorldData::CoordType(rand_.Rand() % 5u + 3u),
			WorldData::CoordType(rand_.Rand() % 3u + 2u),
		};

		WorldData::Segment new_room;
		new_room.type= WorldData::SegmentType::Room;
		new_room.bb_min[2]= result_.segments[segment_index].bb_min[2];
		new_room.bb_max[2]= result_.segments[segment_index].bb_min[2] + room_size[2];

		switch(result_.segments[segment_index].direction)
		{
		case WorldData::Direction::XPlus:
			new_room.bb_min[0]= result_.segments[segment_index].bb_max[0];
			new_room.bb_max[0]= result_.segments[segment_index].bb_max[0] + room_size[0];
			new_room.bb_min[1]= result_.segments[segment_index].bb_max[1] - (room_size[1] >> 1);
			new_room.bb_max[1]= new_room.bb_min[1] + room_size[1];
			break;

		case WorldData::Direction::XMinus:
			new_room.bb_max[0]= result_.segments[segment_index].bb_min[0];
			new_room.bb_min[0]= result_.segments[segment_index].bb_min[0] - room_size[0];
			new_room.bb_min[1]= result_.segments[segment_index].bb_max[1] - (room_size[1] >> 1);
			new_room.bb_max[1]= new_room.bb_min[1] + room_size[1];
			break;

		case WorldData::Direction::YPlus:
			new_room.bb_min[0]= result_.segments[segment_index].bb_max[0] - (room_size[0] >> 1);
			new_room.bb_max[0]= new_room.bb_min[0] + room_size[0];
			new_room.bb_min[1]= result_.segments[segment_index].bb_max[1];
			new_room.bb_max[1]= result_.segments[segment_index].bb_max[1] + room_size[1];
			break;

		case WorldData::Direction::YMinus:
			new_room.bb_min[0]= result_.segments[segment_index].bb_max[0] - (room_size[0] >> 1);
			new_room.bb_max[0]= new_room.bb_min[0] + room_size[0];
			new_room.bb_max[1]= result_.segments[segment_index].bb_min[1];
			new_room.bb_min[1]= result_.segments[segment_index].bb_min[1] - room_size[1];
			break;
		};

		for(size_t i= 0u; i < 3u; ++i)
			KK_ASSERT(new_room.bb_min[0] < new_room.bb_max[0]);

		if(!CanPlace(new_room.bb_min, new_room.bb_max))
			return;

		result_.segments.push_back(new_room);
		Genrate_r(result_.segments.size() - 1u);
	}

	bool CanPlace(const WorldData::CoordType* const bb_min, const WorldData::CoordType* const bb_max)
	{
		for(const WorldData::Segment& segment : result_.segments)
		{
			if( segment.bb_min[0] >= bb_max[0] ||
				segment.bb_min[1] >= bb_max[1] ||
				segment.bb_min[2] >= bb_max[2] ||
				segment.bb_max[0] <= bb_min[0] ||
				segment.bb_max[1] <= bb_min[1] ||
				segment.bb_max[2] <= bb_min[2])
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
