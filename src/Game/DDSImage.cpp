#include "DDSImage.hpp"


namespace KK
{

namespace
{

// Loader of small subset of DDS images.
// https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide

struct DDSPixelFormat
{
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC;
	uint32_t rgb_bit_count;
	uint32_t r_bitmask;
	uint32_t g_bitmask;
	uint32_t b_bitmask;
	uint32_t a_bitmask;
};
static_assert(sizeof(DDSPixelFormat) == 32u, "invalid size");

struct DDSHeader
{
	uint32_t magic;
	uint32_t size;
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitch_or_linear_size;
	uint32_t depth;
	uint32_t mip_count;
	uint32_t reserved1[11];
	DDSPixelFormat pixel_format;
	uint32_t caps;
	uint32_t caps2;
	uint32_t caps3;
	uint32_t caps4;
	uint32_t reserved2;
};
static_assert(sizeof(DDSHeader) == 128u, "invalid size");

namespace DDSPixelFormatFlags
{
	constexpr uint32_t
		DDPF_ALPHAPIXELS= 0x01,
		DDPF_ALPHA= 0x2,
		DDPF_FOURCC= 0x04,
		DDPF_RGB= 0x40,
		DDPF_RGBA= DDPF_RGB | DDPF_ALPHAPIXELS;
}

namespace DDSHeaderFlags
{
	constexpr uint32_t
		DDSD_MIPMAPCOUNT= 0x20000;
}

} // namespace

std::optional<DDSImage> DDSImage::Load(const std::string_view file_name)
{
	auto file= MemoryMappedFile::Create(file_name);
	if(file == nullptr)
		return std::nullopt;

	if(file->Size() < sizeof(DDSHeader))
		return std::nullopt;

	const auto& header= *reinterpret_cast<const DDSHeader*>(file->Data());

	if(header.magic != 0x20534444u)
		return std::nullopt;
	if(header.size != sizeof(DDSHeader) - sizeof(header.magic))
		return std::nullopt;
	if(header.pixel_format.size != sizeof(DDSPixelFormat))
		return std::nullopt;

	// Determine format
	vk::Format format= vk::Format::eUndefined;
	size_t bits_per_pixel= 0u;
	if((header.pixel_format.flags & DDSPixelFormatFlags::DDPF_FOURCC) == DDSPixelFormatFlags::DDPF_FOURCC)
	{
		if(std::memcmp(&header.pixel_format.fourCC, "DXT1", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc1RgbaUnormBlock;
			bits_per_pixel= 4u;
		}
		if(std::memcmp(&header.pixel_format.fourCC, "DXT3", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc2UnormBlock;
			bits_per_pixel= 8u;
		}
		if(std::memcmp(&header.pixel_format.fourCC, "DXT5", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc3UnormBlock;
			bits_per_pixel= 8u;
		}
		if(std::memcmp(&header.pixel_format.fourCC, "BC4U", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc4UnormBlock;
			bits_per_pixel= 4u;
		}
		if(std::memcmp(&header.pixel_format.fourCC, "BC4S", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc4SnormBlock;
			bits_per_pixel= 4u;
		}
		if(std::memcmp(&header.pixel_format.fourCC, "ATI2", sizeof(uint32_t)) == 0u ||
			std::memcmp(&header.pixel_format.fourCC, "BC5U", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc5UnormBlock;
			bits_per_pixel= 8u;
		}
		if(std::memcmp(&header.pixel_format.fourCC, "BC5S", sizeof(uint32_t)) == 0u)
		{
			format= vk::Format::eBc5SnormBlock;
			bits_per_pixel= 8u;
		}

		// TODO - support other formats.
	}
	if((header.pixel_format.flags & DDSPixelFormatFlags::DDPF_RGBA) == DDSPixelFormatFlags::DDPF_RGBA)
	{
		if( header.pixel_format.r_bitmask == 0x000000FFu &&
			header.pixel_format.g_bitmask == 0x0000FF00u &&
			header.pixel_format.b_bitmask == 0x00FF0000u &&
			header.pixel_format.a_bitmask == 0xFF000000u)
		{
			format= vk::Format::eR8G8B8A8Unorm;
			bits_per_pixel= 32u;
		}
		if( header.pixel_format.r_bitmask == 0x00FF0000u &&
			header.pixel_format.g_bitmask == 0x0000FF00u &&
			header.pixel_format.b_bitmask == 0x000000FFu &&
			header.pixel_format.a_bitmask == 0xFF000000u)
		{
			format= vk::Format::eB8G8R8A8Unorm;
			bits_per_pixel= 32u;
		}
	}
	if(format == vk::Format::eUndefined)
		return std::nullopt;

	std::vector<MipLevel> mip_levels;
	if((header.flags & DDSHeaderFlags::DDSD_MIPMAPCOUNT) != 0u)
		mip_levels.resize(header.mip_count);
	else
		mip_levels.resize(1u);

	size_t offset= sizeof(DDSHeader);
	for(size_t i= 0u; i < mip_levels.size(); ++i)
	{
		mip_levels[i].data= static_cast<const char*>(file->Data()) + offset;
		mip_levels[i].size[0]= std::max(header.width  >> i, 1u);
		mip_levels[i].size[1]= std::max(header.height >> i, 1u);

		// Block size is 4x4
		mip_levels[i].size_rounded[0]= (mip_levels[i].size[0] + 3u) & ~3u;
		mip_levels[i].size_rounded[1]= (mip_levels[i].size[1] + 3u) & ~3u;

		mip_levels[i].data_size= mip_levels[i].size_rounded[0] * mip_levels[i].size_rounded[1] * bits_per_pixel / 8u;
		offset+= mip_levels[i].data_size;
	}

	return DDSImage(std::move(file), std::move(mip_levels), format);
}

DDSImage::DDSImage(
	MemoryMappedFilePtr file,
	std::vector<MipLevel> mip_levels,
	vk::Format format)
	: file_(std::move(file))
	, mip_levels_(std::move(mip_levels))
	, format_(std::move(format))
{}

const std::vector<DDSImage::MipLevel>& DDSImage::GetMipLevels() const
{
	return mip_levels_;
}

vk::Format DDSImage::GetFormat() const
{
	return format_;
}

} // namespace KK
