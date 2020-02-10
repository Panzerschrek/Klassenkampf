#pragma once
#include "../Common/MemoryMappedFile.hpp"
#include <vulkan/vulkan.hpp>
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
		size_t data_size= 0u;
	};

	static std::optional<DDSImage> Load(std::string_view file_name);

	DDSImage(DDSImage&&)= default;
	DDSImage& operator=(DDSImage&&)= default;
	DDSImage(const DDSImage&)= delete;
	DDSImage& operator=(const DDSImage&)= delete;

	const std::vector<MipLevel>& GetMipLevels() const;
	vk::Format GetFormat() const;

private:
	DDSImage(MemoryMappedFilePtr file, std::vector<MipLevel> mip_levels, vk::Format format);

private:
	MemoryMappedFilePtr file_;
	std::vector<MipLevel> mip_levels_;
	vk::Format format_;
};


} // namespace
