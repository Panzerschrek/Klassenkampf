#include "WorldGenerator.hpp"
#include "Assert.hpp"
#include "Log.hpp"
#include "Rand.hpp"
#include <algorithm>
#include <map>


namespace KK
{

namespace
{

using Coord3= WorldData::CoordType[3];

struct PathSearchNode;
using PathSearchNodePtr= std::shared_ptr<PathSearchNode>;

struct PathSearchNode
{
	PathSearchNodePtr parent;

	WorldData::Sector sector;
	WorldData::Portal portal;
};

const WorldData::CoordType c_min_room_size_archs = 2;
const WorldData::CoordType c_max_room_size_archs = 5;
const WorldData::CoordType c_min_room_height = 3;
const WorldData::CoordType c_max_room_height = 7;

const WorldData::CoordType c_column_step= 3;
const WorldData::CoordType c_ceiling_height= 2;

const WorldData::CoordType c_min_corridor_length = 2;
const WorldData::CoordType c_max_corridor_length = 10;

const WorldData::CoordType c_min_shaft_height= 2;
const WorldData::CoordType c_max_shaft_height= 6;

class WorldGenerator final
{
public:
	WorldData::World Generate(LongRand::RandResultType seed);

private:
	PathSearchNodePtr GeneratePathIterative(const WorldData::Sector& start_sector, const Coord3& to);

	using SectorAndPortal= std::pair<WorldData::Sector, WorldData::Portal>;

	static std::vector<SectorAndPortal> GenPossibleLinkedSectorsRoom(const WorldData::Sector& sector);
	static std::vector<SectorAndPortal> GenPossibleLinkedSectorsCorridor(const WorldData::Sector& sector);
	static std::vector<SectorAndPortal> GenPossibleLinkedSectorsShaft(const WorldData::Sector& sector);

	bool CanPlace(const WorldData::Sector& sector, const PathSearchNode& parent_path);

	const WorldData::Portal* FindPortal(const WorldData::CoordType* cell);

	void FillSegmentsCorridor(WorldData::Sector& sector);
	void FillSegmentsRoom(WorldData::Sector& sector);
	void FillSegmentsShaft(WorldData::Sector& sector);

private:
	WorldData::World result_;
	LongRand rand_;
};

WorldData::World WorldGenerator::Generate(const LongRand::RandResultType seed)
{
	rand_.SetInnerState(seed);

	WorldData::Sector root_sector;
	root_sector.type= WorldData::SectorType::Room;

	root_sector.bb_min[0]= -3;
	root_sector.bb_min[1]= -3;
	root_sector.bb_min[2]= +0;
	root_sector.bb_max[0]= +3;
	root_sector.bb_max[1]= +3;
	root_sector.bb_max[2]= +16;
	root_sector.ceiling_height= 2;
	root_sector.columns_step= 3;

	const Coord3 dst
	{
		80 + WorldData::CoordType(rand_.Rand() & 31u),
		80 + WorldData::CoordType(rand_.Rand() & 31u),
		-40 - WorldData::CoordType(rand_.Rand() & 15u),
	};

	for(size_t i= 0u; i < 2u; ++i)
	{
		const PathSearchNodePtr path_node= GeneratePathIterative(root_sector, dst);
		for(const PathSearchNode* node= path_node.get(); node != nullptr; node= node->parent.get())
		{
			result_.sectors.push_back(node->sector);
			result_.portals.push_back(node->portal);
		}
	}

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

PathSearchNodePtr WorldGenerator::GeneratePathIterative(const WorldData::Sector& start_sector, const Coord3& to)
{
	/*
	Perform here some kind of best-first search.
	Store wavefront of nodes in map, sort it by priority.
	Priority calculated as distance and random noize.
	*/

	using Priority= WorldData::CoordType;

	std::multimap< Priority, PathSearchNodePtr > node_heap;

	{
		auto node= std::make_shared<PathSearchNode>();
		node->sector= start_sector;
		node_heap.emplace(0, std::move(node));
	}

	const size_t c_max_iterations= 1024u * 1024u;
	size_t iterations= 0u;
	while(!node_heap.empty() && iterations < c_max_iterations)
	{
		++iterations;

		const PathSearchNodePtr node= node_heap.begin()->second;
		node_heap.erase(node_heap.begin());

		const WorldData::Sector& sector= node->sector;

		const WorldData::CoordType c_threshold= 9;
		if(
			std::abs(to[0] * 2 - (sector.bb_max[0] + sector.bb_min[0])) <= c_threshold &&
			std::abs(to[1] * 2 - (sector.bb_max[1] + sector.bb_min[1])) <= c_threshold &&
			std::abs(to[2] * 2 - (sector.bb_max[2] + sector.bb_min[2])) <= c_threshold)
		{
			Log::Info("Path search finished. Iterations: ", iterations, ", heap size: ", node_heap.size());
			return node;
		}

		std::vector<SectorAndPortal> candidate_sectors;

		switch(sector.type)
		{
		case WorldData::SectorType::Room:
			candidate_sectors= GenPossibleLinkedSectorsRoom(sector);
			break;
		case WorldData::SectorType::Corridor:
			candidate_sectors= GenPossibleLinkedSectorsCorridor(sector);
			break;
		case WorldData::SectorType::Shaft:
			candidate_sectors= GenPossibleLinkedSectorsShaft(sector);
			break;
		};

		for(const SectorAndPortal& candidate : candidate_sectors)
		{
			const WorldData::Sector& candidate_sector= candidate.first;
			const WorldData::Portal& candidate_portal= candidate.second;
			if(!CanPlace(candidate_sector, *node))
				continue;


			WorldData::CoordType square_doubled_dist= 0;
			for(size_t j= 0u; j < 3u; ++j)
			{
				const WorldData::CoordType d= 2 * to[j] - (candidate_sector.bb_min[j] + candidate_sector.bb_max[j]);
				square_doubled_dist+= d * d;
			}
			Priority priority= Priority(std::sqrt(double(square_doubled_dist))) / 4; // TODO - remove floating point arithmetic, use integer inly.

			// Add some random to priority
			priority= priority * Priority((rand_.Rand() & 63u) + 32u) + Priority(rand_.Rand() % 1024u);

			auto new_node= std::make_shared<PathSearchNode>();
			new_node->parent= node;
			new_node->sector= candidate_sector;
			new_node->portal= candidate_portal;

			node_heap.emplace(priority, std::move(new_node));
		}

		// Do not consume too much memory, remove low-priority nodes when limit of heap reached.
		const size_t c_max_nodes_in_heap= 512u * 1024u * 1024u / (sizeof(Priority) + sizeof(PathSearchNodePtr) + sizeof(PathSearchNode));

		while(node_heap.size() > c_max_nodes_in_heap)
			node_heap.erase(std::prev(node_heap.end()));
	}

	if(node_heap.empty())
		Log::Info("Nodes heap is empty and result not reached");
	if(iterations >= c_max_iterations)
		Log::Info("Max iterations reached");

	return nullptr;
}

std::vector<WorldGenerator::SectorAndPortal> WorldGenerator::GenPossibleLinkedSectorsRoom(const WorldData::Sector& sector)
{
	KK_ASSERT(sector.type == WorldData::SectorType::Room);

	const Coord3 sector_size
	{
		sector.bb_max[0] - sector.bb_min[0],
		sector.bb_max[1] - sector.bb_min[1],
		sector.bb_max[2] - sector.bb_min[2],
	};

	std::vector<SectorAndPortal> result;

	for(WorldData::CoordType side_x = sector.columns_step / 2; side_x < sector_size[0]; side_x += sector.columns_step)
	for(WorldData::CoordType side_y = 0; side_y < 2; ++side_y)
	for(WorldData::CoordType corridor_length = c_min_corridor_length; corridor_length <= c_max_corridor_length; ++corridor_length)
	{
		WorldData::Sector new_corridor;
		new_corridor.type= WorldData::SectorType::Corridor;

		WorldData::Portal portal;

		if(side_y == 0)
		{
			new_corridor.direction= WorldData::Direction::YPlus;
			new_corridor.bb_min[1]= sector.bb_max[1];
			new_corridor.bb_max[1]= sector.bb_max[1] + corridor_length;

			portal.bb_min[1]= portal.bb_max[1]= new_corridor.bb_min[1];
		}
		else
		{
			new_corridor.direction= WorldData::Direction::YMinus;
			new_corridor.bb_max[1]= sector.bb_min[1];
			new_corridor.bb_min[1]= sector.bb_min[1] - corridor_length;

			portal.bb_min[1]= portal.bb_max[1]= new_corridor.bb_max[1];
		}
		new_corridor.bb_min[0]= sector.bb_min[0] + side_x;
		new_corridor.bb_max[0]= new_corridor.bb_min[0] + 1;

		new_corridor.bb_min[2]= sector.bb_min[2];
		new_corridor.bb_max[2]= new_corridor.bb_min[2] + 1;

		portal.bb_min[0]= new_corridor.bb_min[0];
		portal.bb_max[0]= new_corridor.bb_max[0];

		portal.bb_min[2]= new_corridor.bb_min[2];
		portal.bb_max[2]= new_corridor.bb_max[2];

		result.emplace_back(new_corridor, portal);
	}

	for(WorldData::CoordType side_y = sector.columns_step / 2; side_y < sector_size[1]; side_y += sector.columns_step)
	for(WorldData::CoordType side_x = 0; side_x < 2; ++side_x)
	for(WorldData::CoordType corridor_length = c_min_corridor_length; corridor_length <= c_max_corridor_length; ++corridor_length)
	{
		WorldData::Sector new_corridor;
		new_corridor.type= WorldData::SectorType::Corridor;

		WorldData::Portal portal;

		if(side_x == 0)
		{
			new_corridor.direction= WorldData::Direction::XPlus;
			new_corridor.bb_min[0]= sector.bb_max[0];
			new_corridor.bb_max[0]= sector.bb_max[0] + corridor_length;

			portal.bb_min[0]= portal.bb_max[0]= new_corridor.bb_min[0];
		}
		else
		{
			new_corridor.direction= WorldData::Direction::XMinus;
			new_corridor.bb_max[0]= sector.bb_min[0];
			new_corridor.bb_min[0]= sector.bb_min[0] - corridor_length;

			portal.bb_min[0]= portal.bb_max[0]= new_corridor.bb_max[0];
		}
		new_corridor.bb_min[1]= sector.bb_min[1] + side_y;
		new_corridor.bb_max[1]= new_corridor.bb_min[1] + 1;

		new_corridor.bb_min[2]= sector.bb_min[2];
		new_corridor.bb_max[2]= new_corridor.bb_min[2] + 1;

		portal.bb_min[1]= new_corridor.bb_min[1];
		portal.bb_max[1]= new_corridor.bb_max[1];

		portal.bb_min[2]= new_corridor.bb_min[2];
		portal.bb_max[2]= new_corridor.bb_max[2];

		result.emplace_back(new_corridor, portal);
	}

	for(WorldData::CoordType x= sector.bb_min[0] + 1; x < sector.bb_max[0]; x += c_column_step)
	for(WorldData::CoordType y= sector.bb_min[1] + 1; y < sector.bb_max[1]; y += c_column_step)
	for(WorldData::CoordType h= c_min_shaft_height; h <= c_max_shaft_height; ++h)
	{
		WorldData::Sector new_shaft;
		new_shaft.type= WorldData::SectorType::Shaft;

		WorldData::Portal portal;

		new_shaft.bb_min[0]= x;
		new_shaft.bb_max[0]= new_shaft.bb_min[0] + 1;
		new_shaft.bb_min[1]= y;
		new_shaft.bb_max[1]= new_shaft.bb_min[1] + 1;
		new_shaft.bb_max[2]= sector.bb_min[2];
		new_shaft.bb_min[2]= new_shaft.bb_max[2] - h;

		portal.bb_min[0]= new_shaft.bb_min[0];
		portal.bb_max[0]= new_shaft.bb_max[0];
		portal.bb_min[1]= new_shaft.bb_min[1];
		portal.bb_max[1]= new_shaft.bb_max[1];
		portal.bb_min[2]= portal.bb_max[2]= new_shaft.bb_max[2];

		result.emplace_back(new_shaft, portal);
	}

	return result;
}

std::vector<WorldGenerator::SectorAndPortal> WorldGenerator::GenPossibleLinkedSectorsCorridor(const WorldData::Sector& sector)
{
	KK_ASSERT(sector.type == WorldData::SectorType::Corridor);

	std::vector<SectorAndPortal> result;

	for(WorldData::CoordType arch_x = c_min_room_size_archs; arch_x <= c_max_room_size_archs; ++arch_x)
	for(WorldData::CoordType arch_y = c_min_room_size_archs; arch_y <= c_max_room_size_archs; ++arch_y)
	for(WorldData::CoordType height = c_min_room_height; height <= c_max_room_height; ++height)
	{
		if(sector.direction == WorldData::Direction::XPlus || sector.direction == WorldData::Direction::XMinus)
		{
			for(WorldData::CoordType connection_y = 0; connection_y < arch_y; ++connection_y)
			{
				WorldData::Sector new_room;
				new_room.type= WorldData::SectorType::Room;
				new_room.columns_step= c_column_step;
				new_room.ceiling_height= c_ceiling_height;

				WorldData::Portal portal;

				if(sector.direction == WorldData::Direction::XPlus)
				{
					new_room.bb_min[0]= sector.bb_max[0];
					new_room.bb_max[0]= sector.bb_max[0] + arch_x * c_column_step;

					portal.bb_min[0]= portal.bb_max[0]= sector.bb_max[0];
				}
				else
				{
					new_room.bb_max[0]= sector.bb_min[0];
					new_room.bb_min[0]= sector.bb_min[0] - arch_x * c_column_step;

					portal.bb_min[0]= portal.bb_max[0]= sector.bb_min[0];
				}
				new_room.bb_min[1]= sector.bb_min[1] - connection_y * c_column_step - c_column_step / 2;
				new_room.bb_max[1]= new_room.bb_min[1] + arch_y * c_column_step;
				new_room.bb_min[2]= sector.bb_min[2];
				new_room.bb_max[2]= new_room.bb_min[2] + height;

				portal.bb_min[1]= sector.bb_min[1];
				portal.bb_max[1]= sector.bb_max[1];
				portal.bb_min[2]= sector.bb_min[2];
				portal.bb_max[2]= sector.bb_max[2];

				result.emplace_back(new_room, portal);
			}
		}
		else
		{
			for(WorldData::CoordType connection_x = 0; connection_x < arch_x; ++connection_x)
			{
				WorldData::Sector new_room;
				new_room.type= WorldData::SectorType::Room;
				new_room.columns_step= c_column_step;
				new_room.ceiling_height= c_ceiling_height;

				WorldData::Portal portal;

				if(sector.direction == WorldData::Direction::YPlus)
				{
					new_room.bb_min[1]= sector.bb_max[1];
					new_room.bb_max[1]= sector.bb_max[1] + arch_y * c_column_step;

					portal.bb_min[1]= portal.bb_max[1]= sector.bb_max[1];

				}
				else
				{
					new_room.bb_max[1]= sector.bb_min[1];
					new_room.bb_min[1]= sector.bb_min[1] - arch_y * c_column_step;

					portal.bb_min[1]= portal.bb_max[1]= sector.bb_min[1];
				}
				new_room.bb_min[0]= sector.bb_min[0] - connection_x * c_column_step - c_column_step / 2;
				new_room.bb_max[0]= new_room.bb_min[0] + arch_x * c_column_step;
				new_room.bb_min[2]= sector.bb_min[2];
				new_room.bb_max[2]= new_room.bb_min[2] + height;

				portal.bb_min[0]= sector.bb_min[0];
				portal.bb_max[0]= sector.bb_max[0];
				portal.bb_min[2]= sector.bb_min[2];
				portal.bb_max[2]= sector.bb_max[2];

				result.emplace_back(new_room, portal);
			}
		}
	}

	return result;
}

std::vector<WorldGenerator::SectorAndPortal> WorldGenerator::GenPossibleLinkedSectorsShaft(const WorldData::Sector& sector)
{
	std::vector<SectorAndPortal> result;

	for(WorldData::CoordType arch_x = c_min_room_size_archs; arch_x <= c_max_room_size_archs; ++arch_x)
	for(WorldData::CoordType arch_y = c_min_room_size_archs; arch_y <= c_max_room_size_archs; ++arch_y)
	for(WorldData::CoordType height = c_min_room_height; height <= c_max_room_height; ++height)
	for(WorldData::CoordType connection_x = 0; connection_x < arch_x; ++connection_x)
	for(WorldData::CoordType connection_y = 0; connection_y < arch_y; ++connection_y)
	{
		WorldData::Sector new_room;
		new_room.type= WorldData::SectorType::Room;
		new_room.columns_step= c_column_step;
		new_room.ceiling_height= c_ceiling_height;

		WorldData::Portal portal;

		new_room.bb_min[0]= sector.bb_min[0] - connection_x * c_column_step - c_column_step / 2;
		new_room.bb_max[0]= new_room.bb_min[0] + arch_x * c_column_step;
		new_room.bb_min[1]= sector.bb_min[1] - connection_y * c_column_step - c_column_step / 2;
		new_room.bb_max[1]= new_room.bb_min[1] + arch_y * c_column_step;

		new_room.bb_max[2]= sector.bb_min[2];
		new_room.bb_min[2]= new_room.bb_max[2] - height;

		portal.bb_min[0]= sector.bb_min[0];
		portal.bb_max[0]= sector.bb_max[0];
		portal.bb_min[1]= sector.bb_min[1];
		portal.bb_max[1]= sector.bb_max[1];
		portal.bb_min[2]= portal.bb_max[2]= sector.bb_min[2];

		result.emplace_back(new_room, portal);
	}

	return result;
}

bool WorldGenerator::CanPlace(const WorldData::Sector& sector, const PathSearchNode& parent_path)
{
	const Coord3& bb_min= sector.bb_min;
	const Coord3& bb_max= sector.bb_max;

	for(const WorldData::Sector& check_sector : result_.sectors)
	{
		if( check_sector.bb_min[0] >= bb_max[0] ||
			check_sector.bb_min[1] >= bb_max[1] ||
			check_sector.bb_min[2] >= bb_max[2] ||
			check_sector.bb_max[0] <= bb_min[0] ||
			check_sector.bb_max[1] <= bb_min[1] ||
			check_sector.bb_max[2] <= bb_min[2])
			continue;
		return false;
	}

	for(const PathSearchNode* node= &parent_path; node != nullptr; node= node->parent.get())
	{
		if( node->sector.bb_min[0] >= bb_max[0] ||
			node->sector.bb_min[1] >= bb_max[1] ||
			node->sector.bb_min[2] >= bb_max[2] ||
			node->sector.bb_max[0] <= bb_min[0] ||
			node->sector.bb_max[1] <= bb_min[1] ||
			node->sector.bb_max[2] <= bb_min[2])
			continue;
		return false;
	}

	return true;
}

const WorldData::Portal* WorldGenerator::FindPortal(const WorldData::CoordType* const cell)
{
	for(const WorldData::Portal& portal : result_.portals)
	{
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
	return generator.Generate(9);
}

} // namespace KK
