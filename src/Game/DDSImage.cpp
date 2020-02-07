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
	uint32_t r_bit_mask;
	uint32_t g_bitmask;
	uint32_t b_bit_mask;
	uint32_t a_bit_mask;
};

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

} // namespace

std::optional<DDSImage> DDSImage::Load(const char* const file_name)
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

	std::vector<MipLevel> mip_levels;

	size_t offset= sizeof(DDSHeader);
	mip_levels.resize(header.mip_count);
	for(size_t i= 0u; i < mip_levels.size(); ++i)
	{
		mip_levels[i].data= static_cast<const char*>(file->Data()) + offset;
		mip_levels[i].size[0]= header.width  >> i;
		mip_levels[i].size[1]= header.height >> i;
		offset+= mip_levels[i].size[0] * mip_levels[i].size[1];
	}

	// Skip temporary some mip levels.
	if(mip_levels.size() > 6u)
		mip_levels.resize(6u);

	return DDSImage(std::move(file), std::move(mip_levels));
}

DDSImage::DDSImage(MemoryMappedFilePtr file, std::vector<MipLevel> mip_levels)
	: file_(std::move(file)), mip_levels_(std::move(mip_levels))
{}

const std::vector<DDSImage::MipLevel>& DDSImage::GetMipLevels() const
{
	return mip_levels_;
}

} // namespace KK
