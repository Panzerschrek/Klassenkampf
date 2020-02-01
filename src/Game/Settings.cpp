#include "Settings.hpp"
#include "../Common/MemoryMappedFile.hpp"
#include <cstring>


namespace KK
{

namespace
{

// TODO - use to_chars/from_chars

std::optional<Settings::IntType> StrToInt(const char* str)
{
	Settings::IntType sign= 1;
	if(str[0] == '-')
	{
		sign= -1;
		++str;
	}

	Settings::IntType v= 0;
	while(*str != 0)
	{
		if(str[0] < '0' || str[0] > '9')
			return std::nullopt;
		v*= 10;
		v+= str[0] - '0';
		++str;
	}
	return v * sign;
}

std::optional<Settings::RealType> StrToReal(const char* str)
{
	Settings::RealType sign= 1.0;
	if( str[0] == '-' )
	{
		sign= -1.0;
		++str;
	}

	Settings::RealType v= 0.0;
	while(*str != 0)
	{
		if(str[0] == ',' || str[0] == '.')
		{
			++str;
			break;
		}
		if(str[0] < '0' || str[0] > '9')
			return std::nullopt;
		v*= 10.0;
		v+= float(str[0] - '0');
		++str;
	}
	Settings::RealType m= 0.1f;
	while( *str != 0 )
	{
		if( str[0] < '0' || str[0] > '9' )
			return std::nullopt;

		v+= float(str[0] - '0') * m;
		m*= 0.1f;
		++str;
	}

	return v * sign;
}

std::string NumberToString(const Settings::IntType i)
{
	return std::to_string(i);
}

std::string NumberToString(const Settings::RealType f)
{
	// HACK - replace ',' to '.' for bad locale
	std::string result= std::to_string(f);
	size_t pos = result.find(",");
	if (pos != std::string::npos) result[pos] = '.';

	return result;
}

std::string MakeQuotedString(const std::string& str)
{
	std::string result;
	result.reserve(str.size() + 3u);
	result+= "\"";

	for(const char c : str)
	{
		if(c == '"' || c == '\\')
			result+= '\\';
		result+= c;
	}

	result += "\"";
	return result;
}

} // namespace

Settings::Settings(const std::string_view file_name)
	: file_name_(file_name)
{
	const MemoryMappedFilePtr mapped_file= MemoryMappedFile::Create(file_name_.c_str());
	if(mapped_file == nullptr)
		return;

	const char* s= static_cast<const char*>(mapped_file->Data());
	const char* const s_end= s + mapped_file->Size();
	while(s < s_end)
	{
		std::string str[2]; // key-value pair

		for(size_t i= 0u; i < 2u; ++i)
		{
			while(s < s_end && std::isspace(*s))
				++s;
			if(s == s_end)
				break;

			if(*s == '"') // string in quotes
			{
				++s;
				while(s < s_end && *s != '"')
				{
					if(*s == '\\' && s + 1 < s_end && (s[1] == '"' || s[1] == '\\') )
						++s; // Escaped symbol
					str[i].push_back(*s);
					++s;
				}
				if(*s == '"')
					++s;
				else
					break;
			}
			else
			{
				while(s < s_end && !std::isspace(*s))
				{
					str[i].push_back(*s);
					++s;
				}
			}
		}

		if(!str[0].empty())
			values_map_.emplace(std::move(str[0]), std::move(str[1]));
	}
}

Settings::~Settings()
{
	FILE* const file= std::fopen(file_name_.c_str(), "wb");
	if(file == nullptr)
	{
		// TODO - log warning
		return;
	}

	for(const auto& map_value : values_map_)
	{
		const std::string key= MakeQuotedString(map_value.first);
		const std::string value= MakeQuotedString(map_value.second);

		std::fprintf(file, "%s %s\r\n", key.c_str(), value.c_str());
	}

	std::fclose(file);
}

std::string_view Settings::GetOrSetString(const std::string_view key, const std::string_view default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
	{
		auto it_bool_pair= values_map_.emplace(temp_key_, default_value);
		return it_bool_pair.first->second;
	}
	else
	{
		return it->second;
	}
}

Settings::IntType Settings::GetOrSetInt(const std::string_view key, const IntType default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
	{
		values_map_.emplace(temp_key_, NumberToString(default_value));
		return default_value;
	}
	else
	{
		const auto res= StrToInt(it->second.c_str());
		if(res == std::nullopt)
		{
			it->second= NumberToString(default_value);
			return default_value;
		}
		return *res;
	}
}

Settings::RealType Settings::GetOrSetReal(const std::string_view key, const RealType default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
	{
		values_map_.emplace(temp_key_, NumberToString(default_value));
		return default_value;
	}
	else
	{
		const auto res= StrToReal(it->second.c_str());
		if(res == std::nullopt)
		{
			it->second= NumberToString(default_value);
			return default_value;
		}
		return *res;
	}
}

} // namespace KK
