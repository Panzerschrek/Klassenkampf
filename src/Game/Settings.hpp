#pragma once
#include <string>
#include <unordered_map>
#include <vector>


namespace KK
{

class Settings final
{
public:
	explicit Settings(std::string_view file_name);
	~Settings();

	Settings(const Settings&)= delete;
	Settings& operator=(const Settings&)= delete;

public:
	using IntType= int64_t;
	using RealType= double;

	std::string_view GetOrSetString(std::string_view key, std::string_view default_value= "");
	IntType GetOrSetInt(std::string_view key, IntType default_value= 0);
	RealType GetOrSetReal(std::string_view key, RealType default_value= 0.0);

	std::string_view GetString(std::string_view key, std::string_view default_value= "");
	IntType GetInt(std::string_view key, IntType default_value = 0);
	RealType GetReal(std::string_view key, RealType default_value = 0.0);

	void SetString(std::string_view key, std::string_view value);
	void SetInt(std::string_view key, IntType value);
	void SetReal(std::string_view key, RealType value);

	bool HasValue(std::string_view key);

	std::vector<std::string> GetSettingsKeysStartsWith(std::string_view key_start) const;

private:
	const std::string file_name_;
	std::string temp_key_;
	std::unordered_map<std::string, std::string> values_map_;
};

} // namespace KK
