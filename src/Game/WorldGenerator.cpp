#include "WorldGenerator.hpp"
#include "Assert.hpp"
#include "Rand.hpp"


namespace KK
{

namespace
{

WorldData::PortalKey MakePortalKey(const size_t sector_index0, const size_t sector_index1)
{
	KK_ASSERT(sector_index0 != sector_index1);
	return std::min(sector_index0, sector_index1) << 32u | std::max(sector_index0, sector_index1);
}

class WorldGenerator final
{
public:
	WorldData::World Generate();

private:
	void Genrate_r(size_t current_sector_index);

	void ProcessCorridor(size_t sector_index);
	void ProcessRoom(size_t sector_index);
	void ProcessShaft(size_t sector_index);

	bool CanPlace(const WorldData::CoordType* bb_min, const WorldData::CoordType* bb_max);
	const WorldData::Portal* FindPortal(const WorldData::CoordType* cell);

	void FillSegmentsCorridor(WorldData::Sector& sector);
	void FillSegmentsRoom(WorldData::Sector& sector);
	void FillSegmentsShaft(WorldData::Sector& sector);

private:
	WorldData::World result_;
	LongRand rand_;
};

WorldData::World WorldGenerator::Generate()
{
	{
		WorldData::Sector root_sector;
		root_sector.type= WorldData::SectorType::Room;

		root_sector.bb_min[0]= -3;
		root_sector.bb_min[1]= -3;
		root_sector.bb_min[2]= +0;
		root_sector.bb_max[0]= +3;
		root_sector.bb_max[1]= +3;
		root_sector.bb_max[2]= +8;
		root_sector.ceiling_height= 2;
		root_sector.columns_step= 3;

		result_.sectors.push_back(std::move(root_sector));
	}
	Genrate_r(0u);

	for(WorldData::Sector& sector : result_.sectors)
	{
		switch(sector.type)
		{
		case WorldData::SectorType::Corridor:
			FillSegmentsCorridor(sector);
			break;
		case WorldData::SectorType::Room:
			FillSegmentsRoom(sector);
			break;
		case WorldData::SectorType::Shaft:
			FillSegmentsShaft(sector);
			break;
		}
	}

	return std::move(result_);
}

void WorldGenerator::Genrate_r(const size_t current_sector_index)
{
	if(result_.sectors.size() >= 64u)
		return;

	switch(result_.sectors[current_sector_index].type)
	{
	case WorldData::SectorType::Corridor:
		ProcessCorridor(current_sector_index);
		break;
	case WorldData::SectorType::Room:
		ProcessRoom(current_sector_index);
		break;
	case WorldData::SectorType::Shaft:
		ProcessShaft(current_sector_index);
		break;
	}
}

void WorldGenerator::ProcessCorridor(const size_t sector_index)
{
	if(rand_.RandBool(1u, 5u))
		return;

	const LongRand::RandResultType size_mul= 3u;
	const WorldData::CoordType room_size[]=
	{
		WorldData::CoordType((rand_.Rand() % 4u + 1u) * size_mul),
		WorldData::CoordType((rand_.Rand() % 4u + 1u) * size_mul),
		WorldData::CoordType(rand_.Rand() % 4u + 4u),
	};

	const WorldData::Sector corridor= result_.sectors[sector_index];

	WorldData::Sector new_room;
	new_room.type= WorldData::SectorType::Room;
	new_room.bb_min[2]= corridor.bb_min[2];
	new_room.bb_max[2]= corridor.bb_min[2] + room_size[2];
	new_room.ceiling_height= 2;
	new_room.columns_step= size_mul;

	// Do not place corridors in place of columns.
	WorldData::CoordType room_x= corridor.bb_min[0] - (room_size[0] >> 1);
	if((corridor.bb_min[0] - room_x) % new_room.columns_step == 0)
		--room_x;
	if((corridor.bb_min[0] - room_x) == new_room.columns_step - 1)
		++room_x;
	WorldData::CoordType room_y= corridor.bb_min[1] - (room_size[1] >> 1);
	if((corridor.bb_min[1] - room_y) % new_room.columns_step == 0)
		--room_y;
	if((corridor.bb_min[1] - room_y) % new_room.columns_step == new_room.columns_step - 1)
		++room_y;

	WorldData::Portal portal;
	portal.bb_min[2]= corridor.bb_min[2];
	portal.bb_max[2]= corridor.bb_max[2];

	switch(result_.sectors[sector_index].direction)
	{
	case WorldData::Direction::XPlus:
		new_room.bb_min[0]= corridor.bb_max[0];
		new_room.bb_max[0]= corridor.bb_max[0] + room_size[0];
		new_room.bb_min[1]= room_y;
		new_room.bb_max[1]= new_room.bb_min[1] + room_size[1];
		portal.bb_min[0]= new_room.bb_min[0];
		portal.bb_max[0]= new_room.bb_min[0];
		portal.bb_min[1]= corridor.bb_min[1];
		portal.bb_max[1]= corridor.bb_max[1];
		break;

	case WorldData::Direction::XMinus:
		new_room.bb_max[0]= corridor.bb_min[0];
		new_room.bb_min[0]= corridor.bb_min[0] - room_size[0];
		new_room.bb_min[1]= room_y;
		new_room.bb_max[1]= new_room.bb_min[1] + room_size[1];
		portal.bb_min[0]= new_room.bb_max[0];
		portal.bb_max[0]= new_room.bb_max[0];
		portal.bb_min[1]= corridor.bb_min[1];
		portal.bb_max[1]= corridor.bb_max[1];
		break;

	case WorldData::Direction::YPlus:
		new_room.bb_min[0]= room_x;
		new_room.bb_max[0]= new_room.bb_min[0] + room_size[0];
		new_room.bb_min[1]= corridor.bb_max[1];
		new_room.bb_max[1]= corridor.bb_max[1] + room_size[1];
		portal.bb_min[0]= corridor.bb_min[0];
		portal.bb_max[0]= corridor.bb_max[0];
		portal.bb_min[1]= new_room.bb_min[1];
		portal.bb_max[1]= new_room.bb_min[1];
		break;

	case WorldData::Direction::YMinus:
		new_room.bb_min[0]= room_x;
		new_room.bb_max[0]= new_room.bb_min[0] + room_size[0];
		new_room.bb_max[1]= corridor.bb_min[1];
		new_room.bb_min[1]= corridor.bb_min[1] - room_size[1];
		portal.bb_min[0]= corridor.bb_min[0];
		portal.bb_max[0]= corridor.bb_max[0];
		portal.bb_min[1]= new_room.bb_max[1];
		portal.bb_max[1]= new_room.bb_max[1];
		break;
	};

	for(size_t i= 0u; i < 3u; ++i)
		KK_ASSERT(new_room.bb_min[0] < new_room.bb_max[0]);

	if(!CanPlace(new_room.bb_min, new_room.bb_max))
		return;

	result_.portals.emplace(
		MakePortalKey(sector_index, result_.sectors.size()),
		portal);

	result_.sectors.push_back(new_room);
	Genrate_r(result_.sectors.size() - 1u);
}

void WorldGenerator::ProcessRoom(const size_t sector_index)
{
	const WorldData::Sector sector= result_.sectors[sector_index];

	// Do not place corridors in place of columns.
	WorldData::CoordType corridor_x= sector.bb_min[0] + ((sector.bb_max[0] - sector.bb_min[0]) >> 1);
	if((corridor_x - sector.bb_min[0]) % sector.columns_step == 0)
		++corridor_x;
	if((corridor_x - sector.bb_min[0]) % sector.columns_step == sector.columns_step - 1)
		--corridor_x;

	WorldData::CoordType corridor_y= sector.bb_min[1] + ((sector.bb_max[1] - sector.bb_min[1]) >> 1);
	if((corridor_y - sector.bb_min[1]) % sector.columns_step == 0)
		++corridor_y;
	if((corridor_y - sector.bb_min[1]) % sector.columns_step == sector.columns_step - 1)
		--corridor_y;

	size_t new_sectors_count= 0u;
	for(size_t dir_index= 0u; dir_index < 4u; ++dir_index)
	{
		const WorldData::Direction dir= WorldData::Direction(dir_index);

		if(rand_.RandBool(1u, 4u))
			continue;

		WorldData::Sector new_corridor;
		new_corridor.type= WorldData::SectorType::Corridor;
		new_corridor.direction= dir;
		new_corridor.bb_min[2]= sector.bb_min[2];
		new_corridor.bb_max[2]= sector.bb_min[2] + 1;

		WorldData::Portal portal;
		portal.bb_min[2]= new_corridor.bb_min[2];
		portal.bb_max[2]= new_corridor.bb_max[2];

		const WorldData::CoordType corridor_length= WorldData::CoordType(rand_.Rand() % 7u + 1u);

		switch(dir)
		{
		case WorldData::Direction::XPlus:
			new_corridor.bb_min[0]= sector.bb_max[0];
			new_corridor.bb_max[0]= sector.bb_max[0] + corridor_length;
			new_corridor.bb_min[1]= corridor_y;
			new_corridor.bb_max[1]= new_corridor.bb_min[1] + 1;
			portal.bb_min[0]= new_corridor.bb_min[0];
			portal.bb_max[0]= new_corridor.bb_min[0];
			portal.bb_min[1]= new_corridor.bb_min[1];
			portal.bb_max[1]= new_corridor.bb_max[1];
			break;

		case WorldData::Direction::XMinus:
			new_corridor.bb_max[0]= sector.bb_min[0];
			new_corridor.bb_min[0]= sector.bb_min[0] - corridor_length;
			new_corridor.bb_min[1]= corridor_y;
			new_corridor.bb_max[1]= new_corridor.bb_min[1] + 1;
			portal.bb_min[0]= new_corridor.bb_max[0];
			portal.bb_max[0]= new_corridor.bb_max[0];
			portal.bb_min[1]= new_corridor.bb_min[1];
			portal.bb_max[1]= new_corridor.bb_max[1];
			break;

		case WorldData::Direction::YPlus:
			new_corridor.bb_min[0]= corridor_x;
			new_corridor.bb_max[0]= new_corridor.bb_min[0] + 1;
			new_corridor.bb_min[1]= sector.bb_max[1];
			new_corridor.bb_max[1]= sector.bb_max[1] + corridor_length;
			portal.bb_min[0]= new_corridor.bb_min[0];
			portal.bb_max[0]= new_corridor.bb_max[0];
			portal.bb_min[1]= new_corridor.bb_min[1];
			portal.bb_max[1]= new_corridor.bb_min[1];
			break;

		case WorldData::Direction::YMinus:
			new_corridor.bb_min[0]= corridor_x;
			new_corridor.bb_max[0]= new_corridor.bb_min[0] + 1;
			new_corridor.bb_max[1]= sector.bb_min[1];
			new_corridor.bb_min[1]= sector.bb_min[1] - corridor_length;
			portal.bb_min[0]= new_corridor.bb_min[0];
			portal.bb_max[0]= new_corridor.bb_max[0];
			portal.bb_min[1]= new_corridor.bb_max[1];
			portal.bb_max[1]= new_corridor.bb_max[1];
			break;
		};

		for(size_t i= 0u; i < 3u; ++i)
			KK_ASSERT(new_corridor.bb_min[0] < new_corridor.bb_max[0]);

		if(!CanPlace(new_corridor.bb_min, new_corridor.bb_max))
			continue;

		result_.portals.emplace(
			MakePortalKey(sector_index, result_.sectors.size()),
			portal);

		result_.sectors.push_back(new_corridor);
		++new_sectors_count;
	}

	if(rand_.RandBool(1u, 3u))
	{
		WorldData::Sector new_shaft;
		new_shaft.type= WorldData::SectorType::Shaft;
		new_shaft.direction= WorldData::Direction::XPlus;
		new_shaft.bb_min[0]= (sector.bb_min[0] + sector.bb_max[0]) / 2;
		new_shaft.bb_max[0]= new_shaft.bb_min[0] + 1;
		new_shaft.bb_min[1]= (sector.bb_min[1] + sector.bb_max[1]) / 2;
		new_shaft.bb_max[1]= new_shaft.bb_min[1] + 1;
		new_shaft.bb_max[2]= sector.bb_min[2];
		new_shaft.bb_min[2]= sector.bb_min[2] - (rand_.Rand() % 4 + 1);

		WorldData::Portal portal;
		portal.bb_min[0]= new_shaft.bb_min[0];
		portal.bb_max[0]= new_shaft.bb_max[0];
		portal.bb_min[1]= new_shaft.bb_min[1];
		portal.bb_max[1]= new_shaft.bb_max[1];
		portal.bb_min[2]= new_shaft.bb_max[2];
		portal.bb_max[2]= new_shaft.bb_max[2];

		if(CanPlace(new_shaft.bb_min, new_shaft.bb_max))
		{
			result_.portals.emplace(
				MakePortalKey(sector_index, result_.sectors.size()),
				portal);

			result_.sectors.push_back(new_shaft);
			++new_sectors_count;
		}
	}

	for(size_t i= 0; i < new_sectors_count; ++i)
		Genrate_r(result_.sectors.size() - 1u - i);
}

void WorldGenerator::ProcessShaft(const size_t sector_index)
{
	if(rand_.RandBool(1u, 4u))
		return;

	const WorldData::CoordType x= result_.sectors[sector_index].bb_min[0];
	const WorldData::CoordType y= result_.sectors[sector_index].bb_min[1];
	const WorldData::CoordType z= result_.sectors[sector_index].bb_min[2];

	const LongRand::RandResultType size_mul= 4u;
	const WorldData::CoordType room_size[]=
	{
		WorldData::CoordType((rand_.Rand() % 4u + 1u) * size_mul),
		WorldData::CoordType((rand_.Rand() % 4u + 1u) * size_mul),
		WorldData::CoordType(rand_.Rand() % 4u + 4u),
	};

	WorldData::Sector new_room;
	new_room.type= WorldData::SectorType::Room;
	new_room.direction= WorldData::Direction::XPlus;
	new_room.bb_min[0]= x - (room_size[0] >> 1);
	new_room.bb_max[0]= new_room.bb_min[0] + room_size[0];
	new_room.bb_min[1]= y - (room_size[1] >> 1);
	new_room.bb_max[1]= new_room.bb_min[1] + room_size[1];
	new_room.bb_max[2]= z;
	new_room.bb_min[2]= new_room.bb_max[2] - room_size[2];
	new_room.ceiling_height= 2;
	new_room.columns_step= 4;

	if(!CanPlace(new_room.bb_min, new_room.bb_max))
		return;

	WorldData::Portal portal;
	portal.bb_min[0]= x;
	portal.bb_max[0]= x + 1;
	portal.bb_min[1]= y;
	portal.bb_max[1]= y + 1;
	portal.bb_min[2]= z;
	portal.bb_max[2]= z;

	result_.portals.emplace(
		MakePortalKey(sector_index, result_.sectors.size()),
		portal);

	result_.sectors.push_back(new_room);
	Genrate_r(result_.sectors.size() - 1u);
}

bool WorldGenerator::CanPlace(const WorldData::CoordType* const bb_min, const WorldData::CoordType* const bb_max)
{
	for(const WorldData::Sector& sector : result_.sectors)
	{
		if( sector.bb_min[0] >= bb_max[0] ||
			sector.bb_min[1] >= bb_max[1] ||
			sector.bb_min[2] >= bb_max[2] ||
			sector.bb_max[0] <= bb_min[0] ||
			sector.bb_max[1] <= bb_min[1] ||
			sector.bb_max[2] <= bb_min[2])
			continue;
		return false;
	}

	return true;
}

const WorldData::Portal* WorldGenerator::FindPortal(const WorldData::CoordType* const cell)
{
	for(const auto& portal_pair : result_.portals)
	{
		const WorldData::Portal& portal= portal_pair.second;

		if(portal.bb_min[0] == portal.bb_max[0])
		{
			if(  cell[1] >= portal.bb_min[1] && cell[1] < portal.bb_max[1] &&
				 cell[2] >= portal.bb_min[2] && cell[2] < portal.bb_max[2] &&
				(cell[0] == portal.bb_min[0] || cell[0] + 1 == portal.bb_min[0]))
				return &portal;
		}
		else if(portal.bb_min[1] == portal.bb_max[1])
		{
			if(  cell[0] >= portal.bb_min[0] && cell[0] < portal.bb_max[0] &&
				 cell[2] >= portal.bb_min[2] && cell[2] < portal.bb_max[2] &&
				(cell[1] == portal.bb_min[1] || cell[1] + 1 == portal.bb_min[1]))
				return &portal;
		}
		else if(portal.bb_min[2] == portal.bb_max[2])
		{
			if(  cell[0] >= portal.bb_min[0] && cell[0] < portal.bb_max[0] &&
				 cell[1] >= portal.bb_min[1] && cell[1] < portal.bb_max[1] &&
				(cell[2] == portal.bb_min[2] || cell[2] + 1 == portal.bb_min[2]))
				return &portal;
		}
		else
			KK_ASSERT(false);
	}

	return nullptr;
}

void WorldGenerator::FillSegmentsCorridor(WorldData::Sector& sector)
{
	KK_ASSERT(sector.type == WorldData::SectorType::Corridor);

	WorldData::CoordType pos[2]{ sector.bb_min[0], sector.bb_min[1] };
	WorldData::CoordType delta[2]{ 0, 0 };
	WorldData::CoordType iterations= 0;
	uint8_t angle= 0;
	if(sector.direction == WorldData::Direction::XPlus || sector.direction == WorldData::Direction::XMinus)
	{
		delta[0]= 1;
		delta[1]= 0;
		iterations= sector.bb_max[0] - sector.bb_min[0];
		angle= 1;
	}
	else
	if(sector.direction == WorldData::Direction::YPlus || sector.direction == WorldData::Direction::YMinus)
	{
		delta[0]= 0;
		delta[1]= 1;
		iterations= sector.bb_max[1] - sector.bb_min[1];
		angle= 0;

	}
	else
		KK_ASSERT(false);

	for(WorldData::CoordType i= 0; i < iterations; ++i, pos[0]+= delta[0], pos[1]+= delta[1])
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::Corridor;
		segment.pos[0]= pos[0];
		segment.pos[1]= pos[1];
		segment.pos[2]= sector.bb_min[2];
		segment.angle= angle;
		sector.segments.push_back(std::move(segment));
	}
}

void WorldGenerator::FillSegmentsRoom(WorldData::Sector& sector)
{
	KK_ASSERT(sector.type == WorldData::SectorType::Room);

	// Floors.
	for(WorldData::CoordType x= sector.bb_min[0] + 1; x < sector.bb_max[0] - 1; ++x)
	for(WorldData::CoordType y= sector.bb_min[1] + 1; y < sector.bb_max[1] - 1; ++y)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::Floor;
		segment.pos[0]= x;
		segment.pos[1]= y;
		segment.pos[2]= sector.bb_min[2];
		segment.angle= uint8_t((rand_.Rand() & 255u) / 64u);

		if(FindPortal(segment.pos) != nullptr)
			continue;

		sector.segments.push_back(std::move(segment));
	}

	// Floor-wall joint ±X.
	for(WorldData::CoordType y= sector.bb_min[1] + 1; y < sector.bb_max[1] - 1; ++y)
	for(WorldData::CoordType side= 0; side < 2; ++side)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::FloorWallJoint;
		segment.pos[0]= sector.bb_min[0] + side * (sector.bb_max[0] - sector.bb_min[0] - 1);
		segment.pos[1]= y;
		segment.pos[2]= sector.bb_min[2];
		segment.angle= uint8_t(side * 2);

		if(FindPortal(segment.pos) != nullptr)
			segment.type= WorldData::SegmentType::Floor;

		sector.segments.push_back(std::move(segment));
	}

	// Floor-wall ±Y.
	for(WorldData::CoordType x= sector.bb_min[0] + 1; x < sector.bb_max[0] - 1; ++x)
	for(WorldData::CoordType side= 0; side < 2; ++side)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::FloorWallJoint;
		segment.pos[0]= x;
		segment.pos[1]= sector.bb_min[1] + side * (sector.bb_max[1] - sector.bb_min[1] - 1);
		segment.pos[2]= sector.bb_min[2];
		segment.angle= uint8_t(1 + side * 2);

		if(FindPortal(segment.pos) != nullptr)
			segment.type= WorldData::SegmentType::Floor;

		sector.segments.push_back(std::move(segment));
	}

	// Walls ±X.
	for(WorldData::CoordType y= sector.bb_min[1]; y < sector.bb_max[1]; ++y)
	for(WorldData::CoordType z= sector.bb_min[2] + 1; z < sector.bb_max[2]; ++z)
	for(WorldData::CoordType side= 0; side < 2; ++side)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::Wall;
		segment.pos[0]= sector.bb_min[0] + side * (sector.bb_max[0] - sector.bb_min[0] - 1);
		segment.pos[1]= y;
		segment.pos[2]= z;
		segment.angle= uint8_t(side * 2);
		sector.segments.push_back(std::move(segment));
	}

	// Walls ±Y.
	for(WorldData::CoordType x= sector.bb_min[0]; x < sector.bb_max[0]; ++x)
	for(WorldData::CoordType z= sector.bb_min[2] + 1; z < sector.bb_max[2]; ++z)
	for(WorldData::CoordType side= 0; side < 2; ++side)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::Wall;
		segment.pos[0]= x;
		segment.pos[1]= sector.bb_min[1] + side * (sector.bb_max[1] - sector.bb_min[1] - 1);
		segment.pos[2]= z;
		segment.angle= uint8_t(1 + side * 2);
		sector.segments.push_back(std::move(segment));
	}

	// Lower corners.
	for(WorldData::CoordType side_x= 0; side_x < 2; ++side_x)
	for(WorldData::CoordType side_y= 0; side_y < 2; ++side_y)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::FloorWallWallJoint;
		segment.pos[0]= sector.bb_min[0] + side_x * (sector.bb_max[0] - sector.bb_min[0] - 1);
		segment.pos[1]= sector.bb_min[1] + side_y * (sector.bb_max[1] - sector.bb_min[1] - 1);
		segment.pos[2]= sector.bb_min[2];
		segment.angle= uint8_t(side_x == 0 ? (1 - side_y) : (side_y + 2));
		sector.segments.push_back(std::move(segment));
	}

	// Ceilings.
	for(WorldData::CoordType x= sector.bb_min[0]; x < sector.bb_max[0]; x+= sector.columns_step)
	for(WorldData::CoordType y= sector.bb_min[1]; y < sector.bb_max[1]; y+= sector.columns_step)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::CeilingArch3;
		segment.pos[0]= x;
		segment.pos[1]= y;
		segment.pos[2]= sector.bb_max[2] - sector.ceiling_height;
		segment.angle= 0;
		sector.segments.push_back(std::move(segment));
	}

	// Columns.
	for(WorldData::CoordType x= sector.bb_min[0]; x <= sector.bb_max[0]; x+= sector.columns_step)
	for(WorldData::CoordType y= sector.bb_min[1]; y <= sector.bb_max[1]; y+= sector.columns_step)
	{
		if( x > sector.bb_min[0] && x < sector.bb_max[0] &&
			y > sector.bb_min[1] && y < sector.bb_max[1])
		{
			WorldData::Segment segment;
			segment.type= WorldData::SegmentType::Column3Lights;
			segment.pos[0]= x;
			segment.pos[1]= y;
			segment.pos[2]= sector.bb_min[2];
			segment.angle= 0;
			sector.segments.push_back(std::move(segment));
		}
		for(WorldData::CoordType z= sector.bb_min[2]; z < sector.bb_max[2] - sector.ceiling_height; ++z)
		{
			WorldData::Segment segment;
			segment.type= WorldData::SegmentType::Column3;
			segment.pos[0]= x;
			segment.pos[1]= y;
			segment.pos[2]= z;
			segment.angle= 0;
			sector.segments.push_back(std::move(segment));
		}
	}
}

void WorldGenerator::FillSegmentsShaft(WorldData::Sector& sector)
{
	KK_ASSERT(sector.type == WorldData::SectorType::Shaft);
	for(WorldData::CoordType z= sector.bb_min[2]; z < sector.bb_max[2]; ++z)
	{
		WorldData::Segment segment;
		segment.type= WorldData::SegmentType::Shaft;
		segment.pos[0]= sector.bb_min[0];
		segment.pos[1]= sector.bb_min[1];
		segment.pos[2]= z;
		segment.angle= 0;
		sector.segments.push_back(std::move(segment));
	}
}

} // namespace

WorldData::World GenerateWorld()
{
	WorldGenerator generator;
	return generator.Generate();
}

} // namespace KK
