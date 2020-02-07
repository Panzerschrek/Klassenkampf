#pragma once
#include "../Common/MemoryMappedFile.hpp"
#include <optional>
#include <vector>


namespace KK
{

class DDSImage final
{
public:
	struct MipLevel
	{
		const void* data= nullptr;
		uint32_t size[2]{0, 0};
		uint32_t size_rounded[2]{0, 0};
	};

	static std::optional<DDSImage> Load(const char* file_name);

	DDSImage(DDSImage&&)= default;
	DDSImage& operator=(DDSImage&&)= default;
	DDSImage(const DDSImage&)= delete;
	DDSImage& operator=(const DDSImage&)= delete;

	const std::vector<MipLevel>& GetMipLevels() const;

private:
	DDSImage(MemoryMappedFilePtr file, std::vector<MipLevel> mip_levels);

private:
	MemoryMappedFilePtr file_;
	std::vector<MipLevel> mip_levels_;
};


} // namespace
