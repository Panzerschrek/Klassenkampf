#include "../Common/MemoryMappedFile.hpp"
#include "../Common/SegmentModelFormat.hpp"
#include <tinyxml2.h>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>


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

struct TriangleGroup
{
	std::vector<VertexCombined> vertices;
	std::string material;
};

struct TriangleGroupIndexed
{
	std::vector<VertexCombined> vertices;
	std::vector<uint16_t> indices;
	std::string material;
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
			for(size_t j= 0u; j < std::min(vertex_source->vector_size, size_t(3u)); ++j)
				out_vertex.normal[j]= normal_source->data[ indices[i + normal_offset] * normal_source->vector_size + j ];
		}
		if(tex_coord_source != nullptr)
		{
			out_vertex.tex_coord[0]= out_vertex.tex_coord[1]= 0.0f;
			for(size_t j= 0u; j < std::min(vertex_source->vector_size, size_t(2u)); ++j)
				out_vertex.tex_coord[j]= tex_coord_source->data[ indices[i + tex_coord_offset] * tex_coord_source->vector_size + j ];
		}

		result.vertices.push_back(out_vertex);
	}

	const char* const material= triangle_element.Attribute("material");
	if(material != nullptr)
		result.material= material;

	return result;
}

TriangleGroupIndexed MakeTriangleGroupIndexed(const TriangleGroup& triangle_group)
{
	// TODO - remove duplicated vertices.

	TriangleGroupIndexed result;
	result.material= triangle_group.material;

	for(const VertexCombined& v : triangle_group.vertices)
	{
		result.indices.push_back(uint16_t(result.vertices.size()));
		result.vertices.push_back(v);
	}

	return result;
}

using FileData= std::vector<std::byte>;

FileData DoExport(const std::vector<TriangleGroupIndexed>& triangle_groups)
{
	FileData file_data;

	file_data.resize(file_data.size() + sizeof(KK::SegmentModelFormat::SegmentModelHeader), std::byte(0));
	const auto get_data_file=
		[&]() -> KK::SegmentModelFormat::SegmentModelHeader& { return *reinterpret_cast<KK::SegmentModelFormat::SegmentModelHeader*>( file_data.data() ); };

	std::memcpy(get_data_file().header, KK::SegmentModelFormat::SegmentModelHeader::c_expected_header, sizeof(get_data_file().header));
	get_data_file().version= KK::SegmentModelFormat::SegmentModelHeader::c_expected_version;

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

	const float c_max_coord_value= 32766.0f;
	float inv_scale[3];
	for(size_t i= 0u; i < 3u; ++i)
		inv_scale[i]= (c_max_coord_value * 2.0f) / (bb_max[i] - bb_min[i]);

	for(size_t i= 0u; i < 3u; ++i)
	{
		get_data_file().scale[i]= 1.0f / inv_scale[i];
		get_data_file().shift[i]= 0.0f; // TODO
	}

	// Write triangle groups.
	{
		get_data_file().triangle_groups_offset= uint32_t(file_data.size());
		const auto get_out_triangle_groups=
		[&]() -> KK::SegmentModelFormat::TriangleGroup*
		{
			return reinterpret_cast<KK::SegmentModelFormat::TriangleGroup*>(file_data.data() + get_data_file().triangle_groups_offset);
		};
		file_data.resize(file_data.size() + sizeof(KK::SegmentModelFormat::TriangleGroup) * triangle_groups.size());

		size_t total_vertices= 0u;
		for(const TriangleGroupIndexed& triangle_group : triangle_groups)
		{
			KK::SegmentModelFormat::TriangleGroup& out_group= get_out_triangle_groups()[size_t(get_data_file().triangle_group_count)];
			out_group.first_vertex= uint32_t(total_vertices);
			out_group.index_count= uint16_t(triangle_group.vertices.size());
			total_vertices+= triangle_group.vertices.size();
			++get_data_file().triangle_group_count;
		}
	}

	// Write vertices.
	get_data_file().vertices_offset= uint32_t(file_data.size());
	for(const TriangleGroupIndexed& triangle_group : triangle_groups)
	{
		for(const VertexCombined& vertex : triangle_group.vertices)
		{
			file_data.resize(file_data.size() + sizeof(KK::SegmentModelFormat::Vertex));
			KK::SegmentModelFormat::Vertex& out_vertex= *reinterpret_cast<KK::SegmentModelFormat::Vertex*>(file_data.data() + file_data.size() - sizeof(KK::SegmentModelFormat::Vertex));
			for(size_t i= 0u; i < 3u; ++i)
				out_vertex.pos[i]= int16_t(std::min(std::max(-c_max_coord_value, vertex.pos[i] * inv_scale[i]), +c_max_coord_value));
		}

		get_data_file().vertex_count+= uint32_t(triangle_group.vertices.size());
	}

	// Write indices.
	get_data_file().indices_offset= uint32_t(file_data.size());
	for(const TriangleGroupIndexed& triangle_group : triangle_groups)
	{
		for(const uint16_t index : triangle_group.indices)
		{
			file_data.resize(file_data.size() + sizeof(KK::SegmentModelFormat::IndexType));
			KK::SegmentModelFormat::IndexType& out_index= *reinterpret_cast<KK::SegmentModelFormat::IndexType*>(file_data.data() + file_data.size() - sizeof(KK::SegmentModelFormat::IndexType));
			out_index= index;
		}

		get_data_file().index_count+= uint32_t(triangle_group.indices.size());
	}

	return file_data;
}

void WriteFile(const FileData& content, const char* file_name)
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

int main(int argc, const char* const argv[])
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

	const KK::MemoryMappedFilePtr input_file_mapped= KK::MemoryMappedFile::Create(input_files.front().c_str());
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

	std::vector<TriangleGroupIndexed> triangle_groups;

	const tinyxml2::XMLElement* collada_element= doc.RootElement();//->FirstChildElement("COLLADA");
	if(std::strcmp(collada_element->Name(), "COLLADA") != 0)
		return 1;

	const tinyxml2::XMLElement* library_geometries= collada_element->FirstChildElement("library_geometries");
	for(const tinyxml2::XMLElement* geometry= library_geometries->FirstChildElement("geometry");
		geometry != nullptr;
		geometry= geometry->NextSiblingElement("geometry"))
	{
		const char* const id= geometry->Attribute("id");
		std::cout << "Geometry " << (id == nullptr ? "" : id) << std::endl;

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
				const TriangleGroup triangle_group= ReadTriangleGroup(*triangles, coord_sources);
				if(!triangle_group.vertices.empty())
					triangle_groups.push_back(MakeTriangleGroupIndexed(triangle_group));
			}
		}
	}

	WriteFile(DoExport(triangle_groups), output_file.c_str());
}
