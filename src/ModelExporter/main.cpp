#include <tinyxml2.h>
#include <iostream>


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
	}
}
