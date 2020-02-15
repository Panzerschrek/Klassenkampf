#pragma once
#include <cstdint>


namespace KK
{

namespace SegmentModelFormat
{

constexpr const int32_t c_tex_coord_scale= 1024;

struct SegmentModelHeader
{
	static constexpr const char c_expected_header[16]= "KK-SegmentModel";
	static constexpr const uint32_t c_expected_version= 5u; // Change this each time, when "SegmentModelFormat" structs changed.

	uint8_t header[16];
	uint32_t version;

	// final_position= shift + vertex_position * scale
	float shift[3];
	float scale[3];

	uint32_t vertices_offset;
	uint32_t vertex_count;

	uint32_t indices_offset;
	uint32_t index_count;

	uint32_t triangle_groups_offset;
	uint32_t triangle_group_count;

	uint32_t materials_offset;
	uint32_t material_count;

	uint32_t lights_offset;
	uint32_t light_count;
};
static_assert(sizeof(SegmentModelHeader) == 84u, "Invalid size");

struct Vertex
{
	// Scaled and shifted position. See "shift" and "scale" fields in header.
	int16_t pos[3];
	// Normalized texture coordinates in 4.12 format.
	int16_t tex_coord[2];
	int8_t normal[3];
	int8_t reserved[3];
};
static_assert(sizeof(Vertex) == 16u, "Invalid size");

using IndexType= uint16_t;

struct TriangleGroup
{
	uint32_t first_vertex;
	uint32_t vertex_count;
	uint32_t first_index;
	uint32_t index_count;
	uint32_t material_id;
};
static_assert(sizeof(TriangleGroup) == 20u, "Invalid size");

struct Material
{
	char name[32];
};
static_assert(sizeof(Material) == 32u, "Invalid size");

struct Light
{
	// Pos, radius - in same scale, as vertex data
	int16_t pos[3];
	int16_t radius;
	uint16_t color[3]; // In 8.8 format
};
static_assert(sizeof(Light) == 14u, "Invalid size");

} // namespace ModelFormat

} // namespace KK
