#pragma once
#include <optional>
#include <vector>


namespace KK
{


class Image final
{
public:
	using PixelType= uint32_t;

public:
	static std::optional<Image> Load(const char* file_path);

public:
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	const PixelType* GetData() const;

private:
	Image(uint32_t width, uint32_t height, const PixelType* data);
	Image(uint32_t width, uint32_t height, std::vector<PixelType> data);

private:
	uint32_t size_[2];
	std::vector<PixelType> data_;
};

} // namespace KK
