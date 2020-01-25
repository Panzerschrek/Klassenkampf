#include <tinyxml2.h>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>


struct CoordSource
{
	std::vector<float> data;
	size_t vector_size= 0u;
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

int main()
{
	tinyxml2::XMLDocument doc;
	const tinyxml2::XMLError load_result= doc.LoadFile("test_segment.dae");
	if(load_result != tinyxml2::XML_SUCCESS)
	{
		std::cerr << "XML Parse error: " << doc.ErrorStr() << std::endl;
		return 1;
	}

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
			std::unordered_map<std::string, CoordSource> coord_sources;

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

			std::cout << "Mesh " << coord_sources.size() << " sources" << std::endl;
		}
	}
}
