#include "../Common/MemoryMappedFile.hpp"
#include "../Common/SegmentModelFormat.hpp"
#include "../MathLib/Mat.hpp"
#include <tinyxml2.h>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>


namespace KK
{

struct Light
{
	float color[3];
	float radius;
};

using Lights= std::unordered_map<std::string, Light>;

struct LightTransformed
{
	float pos[3];
	float color[3];
	float radius;
};

using LightsTransformed= std::vector<LightTransformed>;

struct CoordSource
{
	std::vector<float> data;
	size_t vector_size= 0u;
};

using CoordSources= std::unordered_map<std::string, CoordSource>;

struct VertexCombined
{
	float pos[3]{0.0f, 0.0f, 0.0f};
	float normal[3]{0.0f, 0.0f, 0.0f};
	float tex_coord[2]{0.0f, 0.0f};
};

struct VertexWithTangentSpace : public VertexCombined
{
	float binormal[3]{0.0f, 0.0f, 0.0f};
	float tangent[3]{0.0f, 0.0f, 0.0f};
};

bool operator==(const VertexCombined& v0, const VertexCombined& v1)
{
	return
		v0.pos[0] == v1.pos[0] &&
		v0.pos[1] == v1.pos[1] &&
		v0.pos[2] == v1.pos[2] &&
		v0.normal[0] == v1.normal[0] &&
		v0.normal[1] == v1.normal[1] &&
		v0.normal[2] == v1.normal[2] &&
		v0.tex_coord[0] == v1.tex_coord[0] &&
		v0.tex_coord[1] == v1.tex_coord[1];
}

struct VertexCombinedHasher
{
	size_t operator()(const VertexCombined& v) const
	{
		const std::hash<float> hf;
		// TODO - use better hash combine function.
		return
			hf(v.pos[0]) ^
			hf(v.pos[1]) ^
			hf(v.pos[2]) ^
			hf(v.normal[0]) ^
			hf(v.normal[1]) ^
			hf(v.normal[2]) ^
			hf(v.tex_coord[0]) ^
			hf(v.tex_coord[1]);
	}
};

struct TriangleGroup
{
	std::vector<VertexCombined> vertices;
	std::string material;
};

using TriangleGroups= std::vector<TriangleGroup>;
using Geometries= std::unordered_map<std::string, TriangleGroups>;

struct TriangleGroupIndexed
{
	std::vector<VertexWithTangentSpace> vertices;
	std::vector<uint16_t> indices;
	std::string material;
};

struct SceneNode
{
	m_Mat4 transform_matrix;
	std::vector<std::string> geometries_id;
	std::vector<std::string> lights_id;
};

CoordSource ReadCoordSource(const tinyxml2::XMLElement& source_element)
{
	const tinyxml2::XMLElement* const float_array_element= source_element.FirstChildElement("float_array");
	if(float_array_element == nullptr)
		return CoordSource();

	CoordSource result;
	{
		std::istringstream ss(float_array_element->GetText());
		while(!ss.eof())
		{
			float f= 0.0f;
			ss >> f;
			result.data.push_back(f);
		}
	}
	{
		const char* const count= float_array_element->Attribute("count");
		if(count != nullptr)
		{
			const size_t count_value= std::atol(count);
			if(count_value != result.data.size())
				return CoordSource();
		}
	}
	{
		const tinyxml2::XMLElement* technique_common= source_element.FirstChildElement("technique_common");
		if(technique_common == nullptr)
			return CoordSource();

		const tinyxml2::XMLElement* const accessor= technique_common->FirstChildElement("accessor");
		if(accessor == nullptr)
			return CoordSource();

		const char* const stride= accessor->Attribute("stride");
		if(stride == nullptr)
			return CoordSource();
		result.vector_size= std::atol(stride);

		const char* const count= accessor->Attribute("count");
		if(count == nullptr)
			return CoordSource();
		const size_t count_value= std::atol(count);
		if(count_value * result.vector_size != result.data.size())
			return CoordSource();
	}

	return result;
}

TriangleGroup ReadTriangleGroup(
	const tinyxml2::XMLElement& triangle_element,
	const CoordSources& coord_sources)
{
	size_t vertex_attrib_count= 0u;
	size_t vertex_offset= ~0u;
	size_t normal_offset= ~0u;
	size_t tex_coord_offset= ~0u;

	const char* vertex_source_name= "";
	const char* normal_source_name= "";
	const char* tex_coord_source_name= "";

	for(const tinyxml2::XMLElement* input= triangle_element.FirstChildElement("input");
		input != nullptr;
		input= input->NextSiblingElement("input"))
	{
		const char* const semantic= input->Attribute("semantic");
		const char* source= input->Attribute("source");
		const char* const offset= input->Attribute("offset");
		if(semantic == nullptr || source == nullptr || offset == nullptr)
			continue;

		if(source[0] == '#')
			++source;

		const size_t offset_value= std::atol(offset);

		if(std::strcmp(semantic, "VERTEX") == 0)
		{
			vertex_offset= offset_value;
			vertex_source_name= source;
			++vertex_attrib_count;
		}
		if(std::strcmp(semantic, "NORMAL") == 0)
		{
			normal_offset= offset_value;
			normal_source_name= source;
			++vertex_attrib_count;
		}
		if(std::strcmp(semantic, "TEXCOORD") == 0)
		{
			tex_coord_offset= offset_value;
			tex_coord_source_name= source;
			++vertex_attrib_count;
		}
	}

	if(vertex_attrib_count == 0u || vertex_offset == ~0u)
		return TriangleGroup();

	if((vertex_offset != ~0u && vertex_offset >= vertex_attrib_count) ||
		(normal_offset != ~0u && normal_offset >= vertex_attrib_count) ||
		(tex_coord_offset != ~0u && tex_coord_offset >= vertex_attrib_count))
		return TriangleGroup();

	const CoordSource* vertex_source= nullptr;
	const CoordSource* normal_source= nullptr;
	const CoordSource* tex_coord_source= nullptr;

	const auto vertex_source_it= coord_sources.find(vertex_source_name);
	if(vertex_source_name[0] != '\0' && vertex_source_it == coord_sources.end())
		return TriangleGroup();
	else
		vertex_source= &vertex_source_it->second;

	const auto normal_source_it= coord_sources.find(normal_source_name);
	if(normal_source_name[0] != '\0' && normal_source_it == coord_sources.end())
		return TriangleGroup();
	else
		normal_source= &normal_source_it->second;

	const auto tex_coord_source_it= coord_sources.find(tex_coord_source_name);
	if(tex_coord_source_name[0] != '\0' && tex_coord_source_it == coord_sources.end())
		return TriangleGroup();
	else
		tex_coord_source= &tex_coord_source_it->second;

	const tinyxml2::XMLElement* const p_element= triangle_element.FirstChildElement("p");
	if(p_element == nullptr)
		return TriangleGroup();

	std::vector<uint32_t> indices;
	std::istringstream ss(p_element->GetText());
	while(!ss.eof())
	{
		uint32_t i= 0u;
		ss >> i;
		indices.push_back(i);
	}

	if(indices.size() % vertex_attrib_count != 0u)
		return TriangleGroup();

	TriangleGroup result;

	if(const tinyxml2::XMLElement* const vcount_element= triangle_element.FirstChildElement("vcount"))
	{
		std::vector<uint32_t> vcount;
		std::istringstream ss(vcount_element->GetText());
		while(!ss.eof())
		{
			uint32_t c= 0u;
			ss >> c;
			vcount.push_back(c);
		}

		std::vector<VertexCombined> poly_vertices;
		size_t index_index= 0u;
		for(const size_t poly_vertex_count : vcount)
		{
			if(poly_vertex_count < 3u)
				break;

			for(size_t i= 0u; i < poly_vertex_count; ++i)
			{
				const size_t v_i= index_index + i * vertex_attrib_count;

				VertexCombined out_vertex;
				if(vertex_source != nullptr)
				{
					out_vertex.pos[0]= out_vertex.pos[1]= out_vertex.pos[2]= 0.0f;
					for(size_t j= 0u; j < std::min(vertex_source->vector_size, size_t(3u)); ++j)
						out_vertex.pos[j]= vertex_source->data[ indices[v_i + vertex_offset] * vertex_source->vector_size + j ];
				}
				if(normal_source != nullptr)
				{
					out_vertex.normal[0]= out_vertex.normal[1]= out_vertex.normal[2]= 0.0f;
					for(size_t j= 0u; j < std::min(normal_source->vector_size, size_t(3u)); ++j)
						out_vertex.normal[j]= normal_source->data[ indices[v_i + normal_offset] * normal_source->vector_size + j ];
				}
				if(tex_coord_source != nullptr)
				{
					out_vertex.tex_coord[0]= out_vertex.tex_coord[1]= 0.0f;
					for(size_t j= 0u; j < std::min(tex_coord_source->vector_size, size_t(2u)); ++j)
						out_vertex.tex_coord[j]= tex_coord_source->data[ indices[v_i + tex_coord_offset] * tex_coord_source->vector_size + j ];
					out_vertex.tex_coord[1]= 1.0f - out_vertex.tex_coord[1];
				}
				poly_vertices.push_back(out_vertex);
			}

			for(size_t i= 0u; i < poly_vertex_count - 2u; ++i)
			{
				result.vertices.push_back(poly_vertices[0]);
				result.vertices.push_back(poly_vertices[i+1u]);
				result.vertices.push_back(poly_vertices[i+2u]);
			}
			poly_vertices.clear();

			index_index+= poly_vertex_count * vertex_attrib_count;
		}
	}
	else
	{
		for(size_t i= 0u; i < indices.size(); i+= vertex_attrib_count)
		{
			VertexCombined out_vertex;
			if(vertex_source != nullptr)
			{
				out_vertex.pos[0]= out_vertex.pos[1]= out_vertex.pos[2]= 0.0f;
				for(size_t j= 0u; j < std::min(vertex_source->vector_size, size_t(3u)); ++j)
					out_vertex.pos[j]= vertex_source->data[ indices[i + vertex_offset] * vertex_source->vector_size + j ];
			}
			if(normal_source != nullptr)
			{
				out_vertex.normal[0]= out_vertex.normal[1]= out_vertex.normal[2]= 0.0f;
				for(size_t j= 0u; j < std::min(normal_source->vector_size, size_t(3u)); ++j)
					out_vertex.normal[j]= normal_source->data[ indices[i + normal_offset] * normal_source->vector_size + j ];
			}
			if(tex_coord_source != nullptr)
			{
				out_vertex.tex_coord[0]= out_vertex.tex_coord[1]= 0.0f;
				for(size_t j= 0u; j < std::min(tex_coord_source->vector_size, size_t(2u)); ++j)
					out_vertex.tex_coord[j]= tex_coord_source->data[ indices[i + tex_coord_offset] * tex_coord_source->vector_size + j ];
				out_vertex.tex_coord[1]= 1.0f - out_vertex.tex_coord[1];
			}

			result.vertices.push_back(out_vertex);
		}
	}

	const char* const material= triangle_element.Attribute("material");
	if(material != nullptr)
		result.material= material;

	static const char material_namepostfix[] = "-material";
	if(const auto pos = result.material.find(material_namepostfix);
		pos != std::string::npos && pos + std::strlen(material_namepostfix) == result.material.size())
	{
		result.material.erase(pos, result.material.size());
	}

	std::cout << result.material << std::endl;
	return result;
}

TriangleGroupIndexed MakeTriangleGroupIndexed(const TriangleGroup& triangle_group)
{
	struct VertexTangentGroup
	{
		size_t out_index;
		std::vector< std::pair<m_Vec3, m_Vec3> > binormals_tangents;
	};

	std::unordered_map<VertexCombined, std::vector<VertexTangentGroup>, VertexCombinedHasher> vertex_to_index;

	TriangleGroupIndexed result;
	result.material= triangle_group.material;
	for(size_t i= 0u; i < triangle_group.vertices.size(); i+= 3u)
	{
		const VertexCombined& v0= triangle_group.vertices[i + 0u];
		const VertexCombined& v1= triangle_group.vertices[i + 1u];
		const VertexCombined& v2= triangle_group.vertices[i + 2u];

		const m_Vec3 dv0(v1.pos[0] - v0.pos[0], v1.pos[1] - v0.pos[1], v1.pos[2] - v0.pos[2]);
		const m_Vec3 dv1(v2.pos[0] - v0.pos[0], v2.pos[1] - v0.pos[1], v2.pos[2] - v0.pos[2]);
		const m_Vec3 dv2= mVec3Cross(dv0, dv1);

		const m_Vec2 dtc0(v1.tex_coord[0] - v0.tex_coord[0], v1.tex_coord[1] - v0.tex_coord[1]);
		const m_Vec2 dtc1(v2.tex_coord[0] - v0.tex_coord[0], v2.tex_coord[1] - v0.tex_coord[1]);

		m_Mat3 delta_vec_mat;
		delta_vec_mat.value[0]= dv0.x;
		delta_vec_mat.value[1]= dv0.y;
		delta_vec_mat.value[2]= dv0.z;
		delta_vec_mat.value[3]= dv1.x;
		delta_vec_mat.value[4]= dv1.y;
		delta_vec_mat.value[5]= dv1.z;
		delta_vec_mat.value[6]= dv2.x;
		delta_vec_mat.value[7]= dv2.y;
		delta_vec_mat.value[8]= dv2.z;

		const m_Mat3 delta_vec_mat_iverse= delta_vec_mat.GetInverseMatrix();

		m_Vec3 b(
			delta_vec_mat_iverse.value[0] * dtc0.x + delta_vec_mat_iverse.value[1] * dtc1.x,
			delta_vec_mat_iverse.value[3] * dtc0.x + delta_vec_mat_iverse.value[4] * dtc1.x,
			delta_vec_mat_iverse.value[6] * dtc0.x + delta_vec_mat_iverse.value[7] * dtc1.x);
		m_Vec3 t(
			delta_vec_mat_iverse.value[0] * dtc0.y + delta_vec_mat_iverse.value[1] * dtc1.y,
			delta_vec_mat_iverse.value[3] * dtc0.y + delta_vec_mat_iverse.value[4] * dtc1.y,
			delta_vec_mat_iverse.value[6] * dtc0.y + delta_vec_mat_iverse.value[7] * dtc1.y);

		// Normalize it.
		const float max_l= std::max(b.GetLength(), t.GetLength());
		b/= max_l;
		t/= max_l;

		for(size_t j= 0u; j < 3u; ++j)
		{
			std::vector<VertexTangentGroup>& vertex_tangent_groups= vertex_to_index[triangle_group.vertices[i + j]];
			VertexTangentGroup* dst_group= nullptr;

			for(VertexTangentGroup& group : vertex_tangent_groups)
			{
				bool all_ok= true;
				for(const std::pair<m_Vec3, m_Vec3>& bt : group.binormals_tangents)
					all_ok= all_ok && mVec3Dot(bt.first, b) > 0.0f && mVec3Dot(bt.second, t) > 0.0f;
				if(all_ok)
				{
					dst_group= &group;
					break;
				}
			}

			if(dst_group == nullptr)
			{
				vertex_tangent_groups.emplace_back();
				dst_group= &vertex_tangent_groups.back();
				dst_group->out_index= result.vertices.size();

				VertexWithTangentSpace out_v;
				static_cast<VertexCombined&>(out_v)= triangle_group.vertices[i + j];
				result.vertices.push_back(out_v);
			}

			dst_group->binormals_tangents.push_back(std::make_pair(b, t));
			result.indices.push_back(uint16_t(dst_group->out_index));

		} // for thriangle vertices
	} // for triangles

	for(const auto& groups_pair : vertex_to_index)
	for(const VertexTangentGroup& group : groups_pair.second)
	{
		m_Vec3 b(0.0f, 0.0f, 0.0f);
		m_Vec3 t(0.0f, 0.0f, 0.0f);
		for(const std::pair<m_Vec3, m_Vec3>& bt : group.binormals_tangents)
		{
			b+= bt.first;
			t+= bt.second;
		}

		const float max_l= std::max(b.GetLength(), t.GetLength());
		b/= max_l;
		t/= max_l;

		result.vertices[group.out_index].binormal[0]= b.x;
		result.vertices[group.out_index].binormal[1]= b.y;
		result.vertices[group.out_index].binormal[2]= b.z;
		result.vertices[group.out_index].tangent [0]= t.x;
		result.vertices[group.out_index].tangent [1]= t.y;
		result.vertices[group.out_index].tangent [2]= t.z;
	}

	return result;
}

using FileData= std::vector<std::byte>;

FileData DoExport(const std::vector<TriangleGroupIndexed>& triangle_groups, const LightsTransformed& lights)
{
	FileData file_data;

	file_data.resize(file_data.size() + sizeof(SegmentModelFormat::SegmentModelHeader), std::byte(0));
	const auto get_data_file=
		[&]() -> SegmentModelFormat::SegmentModelHeader& { return *reinterpret_cast<SegmentModelFormat::SegmentModelHeader*>( file_data.data() ); };

	std::memcpy(get_data_file().header, SegmentModelFormat::SegmentModelHeader::c_expected_header, sizeof(get_data_file().header));
	get_data_file().version= SegmentModelFormat::SegmentModelHeader::c_expected_version;

	const float inf= 1e24f;
	float bb_min[3] { +inf, +inf, +inf };
	float bb_max[3] { -inf, -inf, -inf };

	for(const TriangleGroupIndexed& triangle_group : triangle_groups)
	{
		for(const VertexCombined& vertex : triangle_group.vertices)
		{
			for(size_t i= 0u; i < 3u; ++i)
			{
				bb_min[i]= std::min(bb_min[i], vertex.pos[i]);
				bb_max[i]= std::max(bb_max[i], vertex.pos[i]);
			}
		}
	}
	for(const LightTransformed& light : lights)
	{
		for(size_t i= 0u; i < 3u; ++i)
		{
			bb_min[i]= std::min(bb_min[i], light.pos[i] - light.radius);
			bb_max[i]= std::max(bb_max[i], light.pos[i] + light.radius);
		}
	}

	// Fix zero size extents.
	for(size_t i= 0u; i < 3u; ++i)
	{
		if(bb_max[i] - bb_min[i] < 0.125f)
			bb_max[i]= bb_min[i] + 0.125f;
	}

	const float c_max_coord_value= 32766.0f;
	const float c_max_normal_value= 126.0f;
	float inv_scale[3];
	for(size_t i= 0u; i < 3u; ++i)
		inv_scale[i]= (c_max_coord_value * 2.0f) / (bb_max[i] - bb_min[i]);

	for(size_t i= 0u; i < 3u; ++i)
	{
		get_data_file().scale[i]= 1.0f / inv_scale[i];
		get_data_file().shift[i]= bb_min[i] + c_max_coord_value / inv_scale[i];
	}

	// Write materials.
	std::unordered_map<std::string, uint32_t> material_name_to_id;
	for(const TriangleGroupIndexed& triangle_group : triangle_groups)
	{
		const auto it= material_name_to_id.find(triangle_group.material);
		if(it == material_name_to_id.end())
		{
			material_name_to_id.emplace(triangle_group.material, uint32_t(material_name_to_id.size()));
		}
	}
	{
		get_data_file().materials_offset= uint32_t(file_data.size());
		get_data_file().material_count= uint32_t(material_name_to_id.size());
		const auto get_out_materials=
		[&]() -> SegmentModelFormat::Material*
		{
			return reinterpret_cast<SegmentModelFormat::Material*>(file_data.data() + get_data_file().materials_offset);
		};
		file_data.resize(file_data.size() + sizeof(SegmentModelFormat::Material) * material_name_to_id.size());

		for(const auto& material_pair : material_name_to_id)
		{
			std::memset(
				get_out_materials()[material_pair.second].name,
				0,
				sizeof(SegmentModelFormat::Material::name));
			std::memcpy(
				get_out_materials()[material_pair.second].name,
				material_pair.first.data(),
				std::min(sizeof(SegmentModelFormat::Material::name), material_pair.first.size()));
		}
	}

	// Write triangle groups.
	{
		get_data_file().triangle_groups_offset= uint32_t(file_data.size());
		const auto get_out_triangle_groups=
		[&]() -> SegmentModelFormat::TriangleGroup*
		{
			return reinterpret_cast<SegmentModelFormat::TriangleGroup*>(file_data.data() + get_data_file().triangle_groups_offset);
		};
		file_data.resize(file_data.size() + sizeof(SegmentModelFormat::TriangleGroup) * triangle_groups.size());

		size_t total_vertices= 0u;
		size_t total_indices= 0u;
		for(const TriangleGroupIndexed& triangle_group : triangle_groups)
		{
			SegmentModelFormat::TriangleGroup& out_group= get_out_triangle_groups()[size_t(get_data_file().triangle_group_count)];
			out_group.first_vertex= uint32_t(total_vertices);
			out_group.vertex_count= uint32_t(triangle_group.vertices.size());
			out_group.first_index= uint32_t(total_indices);
			out_group.index_count= uint32_t(triangle_group.indices.size());
			out_group.material_id= material_name_to_id[triangle_group.material];
			total_vertices+= triangle_group.vertices.size();
			total_indices+= triangle_group.indices.size();
			++get_data_file().triangle_group_count;
		}
	}

	// Write vertices.
	get_data_file().vertices_offset= uint32_t(file_data.size());
	for(const TriangleGroupIndexed& triangle_group : triangle_groups)
	{
		for(const VertexWithTangentSpace& vertex : triangle_group.vertices)
		{
			file_data.resize(file_data.size() + sizeof(SegmentModelFormat::Vertex));
			SegmentModelFormat::Vertex& out_vertex= *reinterpret_cast<SegmentModelFormat::Vertex*>(file_data.data() + file_data.size() - sizeof(SegmentModelFormat::Vertex));

			for(size_t i= 0u; i < 3u; ++i)
			{
				const float pos_transformed= (vertex.pos[i] - bb_min[i]) * inv_scale[i] - c_max_coord_value;
				out_vertex.pos[i]= int16_t(std::min(std::max(-c_max_coord_value, pos_transformed), +c_max_coord_value));
			}
			for(size_t i= 0u; i < 2u; ++i)
				out_vertex.tex_coord[i]= int16_t(vertex.tex_coord[i] * float(SegmentModelFormat::c_tex_coord_scale));
			for(size_t i= 0u; i < 3u; ++i)
			{
				const float normal_scaled= c_max_normal_value * vertex.normal[i];
				out_vertex.normal[i]= int8_t(std::min(std::max(-c_max_normal_value, normal_scaled), +c_max_normal_value));

				const float binormal_scaled= c_max_normal_value * vertex.binormal[i];
				out_vertex.binormal[i]= int8_t(std::min(std::max(-c_max_normal_value, binormal_scaled), +c_max_normal_value));

				const float tangent_scaled= c_max_normal_value * vertex.tangent[i];
				out_vertex.tangent[i]= int8_t(std::min(std::max(-c_max_normal_value, tangent_scaled), +c_max_normal_value));
			}
		}

		get_data_file().vertex_count+= uint32_t(triangle_group.vertices.size());
	}

	// Write indices.
	get_data_file().indices_offset= uint32_t(file_data.size());
	for(const TriangleGroupIndexed& triangle_group : triangle_groups)
	{
		for(const uint16_t index : triangle_group.indices)
		{
			file_data.resize(file_data.size() + sizeof(SegmentModelFormat::IndexType));
			SegmentModelFormat::IndexType& out_index= *reinterpret_cast<SegmentModelFormat::IndexType*>(file_data.data() + file_data.size() - sizeof(SegmentModelFormat::IndexType));
			out_index= index;
		}

		get_data_file().index_count+= uint32_t(triangle_group.indices.size());
	}

	// Write lights
	get_data_file().lights_offset= uint32_t(file_data.size());
	get_data_file().light_count= uint32_t(lights.size());

	const float inv_max_scale= std::min(std::max(inv_scale[0], inv_scale[1]), inv_scale[2]);
	for(const LightTransformed& light : lights)
	{
		file_data.resize(file_data.size() + sizeof(SegmentModelFormat::Light));
		SegmentModelFormat::Light& out_light= *reinterpret_cast<SegmentModelFormat::Light*>(file_data.data() + file_data.size() - sizeof(SegmentModelFormat::Light));

		for(size_t i= 0u; i < 3u; ++i)
		{
			const float pos_transformed= (light.pos[i] - bb_min[i]) * inv_scale[i] - c_max_coord_value;
			out_light.pos[i]= int16_t(std::min(std::max(-c_max_coord_value, pos_transformed), +c_max_coord_value));
		}
		for(size_t i= 0u; i < 3u; ++i)
			out_light.color[i]= int16_t(light.color[i] * 256.0f);

		out_light.radius= int16_t(std::min(std::max(-c_max_coord_value, light.radius * inv_max_scale), +c_max_coord_value));
	}

	std::cout << "Out stats:" << std::endl;
	std::cout << "vertices:	" << get_data_file().vertex_count << "\n";
	std::cout << "triangles: " << get_data_file().index_count / 3u << "\n";
	std::cout << "triangle groups: " << get_data_file().triangle_group_count << "\n";
	std::cout << "materials: " << get_data_file().material_count << "\n";
	std::cout << "lights: " << get_data_file().light_count << "\n";

	return file_data;
}

void WriteFile(const FileData& content, const char* const file_name)
{
	std::FILE* const f= std::fopen(file_name, "wb");
	if(f == nullptr)
	{
		std::cerr << "Error, opening file \"" << file_name << "\"" << std::endl;
		return;
	}

	size_t write_total= 0u;
	do
	{
		const size_t write= std::fwrite(content.data() + write_total, 1, content.size() - write_total, f);
		if(write == 0)
			break;

		write_total+= write;
	} while(write_total < content.size());
}

Geometries ReadGeometries(const tinyxml2::XMLElement& collada_element)
{
	Geometries geometries;

	const tinyxml2::XMLElement* const library_geometries= collada_element.FirstChildElement("library_geometries");
	if(library_geometries == nullptr)
		return geometries;

	for(const tinyxml2::XMLElement* geometry= library_geometries->FirstChildElement("geometry");
		geometry != nullptr;
		geometry= geometry->NextSiblingElement("geometry"))
	{
		const char* const id= geometry->Attribute("id");
		if(id == nullptr)
			continue;

		TriangleGroups triangle_groups;

		for(const tinyxml2::XMLElement* mesh= geometry->FirstChildElement("mesh");
			mesh != nullptr;
			mesh= mesh->NextSiblingElement("mesh"))
		{
			CoordSources coord_sources;

			for(const tinyxml2::XMLElement* source= mesh->FirstChildElement("source");
				source != nullptr;
				source= source->NextSiblingElement("source"))
			{
				const char* const id= source->Attribute("id");
				if(id == nullptr)
					continue;

				CoordSource coord_source= ReadCoordSource(*source);
				if(coord_source.data.empty())
					continue;
				coord_sources[id]= std::move(coord_source);
			}

			for(const tinyxml2::XMLElement* vertices= mesh->FirstChildElement("vertices");
				vertices != nullptr;
				vertices= vertices->NextSiblingElement("vertices"))
			{
				const char* const id= vertices->Attribute("id");
				if(id == nullptr)
					continue;

				const tinyxml2::XMLElement* const input= vertices->FirstChildElement("input");
				if(input == nullptr)
					continue;
				const char* source= input->Attribute("source");
				if(source == nullptr)
					continue;
				if(source[0] == '#')
					++source;

				const auto source_it= coord_sources.find(source);
				if(source_it != coord_sources.end())
				{
					CoordSource source_alias= source_it->second;
					coord_sources[id]= std::move(source_alias);
				}
			}

			for(const tinyxml2::XMLElement* triangles= mesh->FirstChildElement("triangles");
				triangles != nullptr;
				triangles= triangles->NextSiblingElement("triangles"))
			{
				TriangleGroup triangle_group= ReadTriangleGroup(*triangles, coord_sources);
				if(!triangle_group.vertices.empty())
					triangle_groups.push_back(std::move(triangle_group));
			}

			for(const tinyxml2::XMLElement* polylist= mesh->FirstChildElement("polylist");
				polylist != nullptr;
				polylist= polylist->NextSiblingElement("polylist"))
			{
				TriangleGroup triangle_group= ReadTriangleGroup(*polylist, coord_sources);
				if(!triangle_group.vertices.empty())
					triangle_groups.push_back(std::move(triangle_group));
			}
		}

		geometries[id]= std::move(triangle_groups);
	}

	return geometries;
}

Lights ReadLights(const tinyxml2::XMLElement& collada_element)
{
	Lights lights;

	const tinyxml2::XMLElement* const library_lights= collada_element.FirstChildElement("library_lights");
	if(library_lights == nullptr)
		return lights;

	for(const tinyxml2::XMLElement* light= library_lights->FirstChildElement("light");
		light != nullptr;
		light= light->NextSiblingElement("light"))
	{
		const char* const id= light->Attribute("id");
		if(id == nullptr)
			continue;

		const tinyxml2::XMLElement* const technique_common= light->FirstChildElement("technique_common");
		if(technique_common == nullptr)
			continue;
		const tinyxml2::XMLElement* const point= technique_common->FirstChildElement("point");
		if(point == nullptr)
			continue;
		const tinyxml2::XMLElement* const color= point->FirstChildElement("color");
		if(color == nullptr)
			continue;
		const char* const color_text= color->GetText();
		if(color_text == nullptr )
			continue;

		Light out_light;
		out_light.radius= 1.0f;
		out_light.color[0]= out_light.color[1]= out_light.color[2]= 0.0f;

		std::istringstream ss(color_text);
		for(size_t i= 0u; i < 3u; ++i)
			ss >> out_light.color[i];

		// Hack! Use "area_size" property from blender as radius.
		if(const tinyxml2::XMLElement* const extra= light->FirstChildElement("extra"))
		{
			if(const tinyxml2::XMLElement* const technique= extra->FirstChildElement("technique"))
			{
				if(const char* const profile= technique->Attribute("profile"))
				{
					if(std::strcmp(profile, "blender") == 0)
					{
						if(const tinyxml2::XMLElement* const area_size= technique->FirstChildElement("area_size"))
						{
							if(const char* const area_size_value= area_size->GetText())
							{
								std::istringstream ss(area_size_value);
								ss >> out_light.radius;
							}
						}
					}
				}
			}
		}
		lights[id]= std::move(out_light);
	}

	return lights;
}

std::vector<SceneNode> ReadScene(const tinyxml2::XMLElement& collada_element)
{
	std::vector<SceneNode> out_nodes;

	const tinyxml2::XMLElement* library_visual_sceles= collada_element.FirstChildElement("library_visual_scenes");
	if(library_visual_sceles == nullptr)
		return out_nodes;

	for(const tinyxml2::XMLElement* visual_scene= library_visual_sceles->FirstChildElement("visual_scene");
		visual_scene != nullptr;
		visual_scene= visual_scene->NextSiblingElement("visual_scene"))
	{
		for(const tinyxml2::XMLElement* node= visual_scene->FirstChildElement("node");
			node != nullptr;
			node= node->NextSiblingElement("node"))
		{
			SceneNode out_node;

			const tinyxml2::XMLElement* const matrix= node->FirstChildElement("matrix");
			if(matrix == nullptr)
				continue;

			size_t i= 0u;
			std::istringstream ss(matrix->GetText());
			while(!ss.eof() && i < 16u)
			{
				ss >> out_node.transform_matrix.value[i];
				++i;
			}
			if(i != 16u)
				continue;
			out_node.transform_matrix.Transpose();

			for(const tinyxml2::XMLElement* instance_geometry= node->FirstChildElement("instance_geometry");
				instance_geometry != nullptr;
				instance_geometry= instance_geometry->NextSiblingElement("instance_geometry"))
			{
				const char* url= instance_geometry->Attribute("url");
				if(url == nullptr)
					continue;
				if(url[0] == '#')
					++url;
				out_node.geometries_id.push_back(url);
			}

			for(const tinyxml2::XMLElement* instance_light= node->FirstChildElement("instance_light");
				instance_light != nullptr;
				instance_light= instance_light->NextSiblingElement("instance_light"))
			{
				const char* url= instance_light->Attribute("url");
				if(url == nullptr)
					continue;
				if(url[0] == '#')
					++url;
				out_node.lights_id.push_back(url);
			}

			out_nodes.push_back(std::move(out_node));
		}
	}

	return out_nodes;
}

int Main(const int argc, const char* const argv[])
{
	std::vector<std::string> input_files;
	std::string output_file;

	// Parse command line
	#define EXPECT_ARG_VALUE if(i + 1 >= argc) { std::cerr << "Expeted name after \"" << argv[i] << "\"" << std::endl; return -1; }
	for(int i = 1; i < argc;)
	{
		if( std::strcmp( argv[i], "-o" ) == 0 )
		{
			EXPECT_ARG_VALUE
			output_file= argv[ i + 1 ];
			i+= 2;
		}
		else if( std::strcmp( argv[i], "-i" ) == 0 )
		{
			EXPECT_ARG_VALUE
			input_files.push_back( argv[ i + 1 ] );
			i+= 2;
		}
		else
		{
			std::cout << "unknown option: \"" << argv[i] << "\"" << std::endl;
			++i;
		}
	}

	if(input_files.empty())
	{
		std::cerr << "No input files" << std::endl;
		return -1;
	}
	if(input_files.size() > 1u)
	{
		std::cout << "Multiple input files not suppored" << std::endl;
	}
	if(output_file.empty())
	{
		std::cerr << "Output file name not specified" << std::endl;
		return -1;
	}

	const MemoryMappedFilePtr input_file_mapped= MemoryMappedFile::Create(input_files.front());
	if(input_file_mapped == nullptr)
		return -1;

	tinyxml2::XMLDocument doc;
	const tinyxml2::XMLError load_result=
		doc.Parse(
			static_cast<const char*>(input_file_mapped->Data()),
			input_file_mapped->Size());
	if(load_result != tinyxml2::XML_SUCCESS)
	{
		std::cerr << "XML Parse error: " << doc.ErrorStr() << std::endl;
		return 1;
	}

	const tinyxml2::XMLElement* collada_element= doc.RootElement();
	if(std::strcmp(collada_element->Name(), "COLLADA") != 0)
	{
		std::cerr << "Document is not in COLLADA format" << std::endl;
		return 1;
	}

	const Geometries geometries= ReadGeometries(*collada_element);
	const Lights lights= ReadLights(*collada_element);
	const std::vector<SceneNode> scene_nodes= ReadScene(*collada_element);

	std::vector<TriangleGroupIndexed> triangle_groups;
	LightsTransformed lights_transformed;
	for(const SceneNode& node : scene_nodes)
	{
		m_Mat4 normals_matrix= node.transform_matrix;
		normals_matrix.value[12]= normals_matrix.value[13]= normals_matrix.value[14]= 0.0f;
		normals_matrix.Inverse();

		for(const std::string& geometry_id : node.geometries_id)
		{
			const auto it= geometries.find(geometry_id);
			if(it == geometries.end())
				continue;

			const TriangleGroups& geometry_triangle_groups= it->second;
			for(const TriangleGroup& geometry_triangle_group : geometry_triangle_groups)
			{
				TriangleGroup group_copy= geometry_triangle_group;
				for(VertexCombined& v : group_copy.vertices)
				{
					const m_Vec3 pos(v.pos[0], v.pos[1], v.pos[2]);
					const m_Vec3 pos_transformed= pos * node.transform_matrix;
					v.pos[0]= pos_transformed.x;
					v.pos[1]= pos_transformed.y;
					v.pos[2]= pos_transformed.z;

					const m_Vec3 normal(v.normal[0], v.normal[1], v.normal[2]);

					m_Vec3 normal_transformed= normals_matrix * normal;
					normal_transformed/= normal_transformed.GetLength();

					v.normal[0]= normal_transformed.x;
					v.normal[1]= normal_transformed.y;
					v.normal[2]= normal_transformed.z;
				}
				triangle_groups.push_back(MakeTriangleGroupIndexed(group_copy));
			}
		}

		for(const std::string& light_id : node.lights_id)
		{
			const auto it= lights.find(light_id);
			if(it == lights.end())
				continue;

			const Light& in_light= it->second;
			const m_Vec3 pos_transformed= m_Vec3(0.0f, 0.0f, 0.0f) * node.transform_matrix;

			LightTransformed out_light;
			out_light.pos[0]= pos_transformed.x;
			out_light.pos[1]= pos_transformed.y;
			out_light.pos[2]= pos_transformed.z;
			out_light.color[0]= in_light.color[0];
			out_light.color[1]= in_light.color[1];
			out_light.color[2]= in_light.color[2];
			out_light.radius= in_light.radius;

			lights_transformed.push_back(std::move(out_light));
		}
	}

	const FileData out_file_data= DoExport(triangle_groups, lights_transformed);
	WriteFile(out_file_data, output_file.c_str());

	return 0;
}

} // namespace KK

int main(const int argc, const char* const argv[])
{
	return KK::Main(argc, argv);
}
