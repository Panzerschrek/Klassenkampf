#include "Image.hpp"

// This is single place, where this library included.
// So, we can include implementation here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#define STB_IMAGE_IMPLEMENTATION
#include "../stb/stb_image.h"
#pragma GCC diagnostic pop

namespace KK
{

std::optional<Image> Image::Load(const char* const file_path)
{
	int width, height, channels;
	unsigned char* stbi_img_data= stbi_load(file_path, &width, &height, &channels, 0);
	if(stbi_img_data == nullptr)
		return std::nullopt;

	if(channels == 4)
	{
		Image result(uint32_t(width), uint32_t(height), reinterpret_cast<const PixelType*>(stbi_img_data));
		stbi_image_free(stbi_img_data);
		return std::move(result);
	}
	if( channels == 3)
	{
		std::vector<PixelType> data(width * height);
		for(int i= 0; i < width * height; ++i)
			data[i]=
				(stbi_img_data[i*3  ]<< 0) |
				(stbi_img_data[i*3+1]<< 8) |
				(stbi_img_data[i*3+2]<<16) |
				(255                << 24);

		stbi_image_free(stbi_img_data);
		return Image(uint32_t(width), uint32_t(height), std::move(data));
	}
	// TODO - change to 4 components format

	stbi_image_free(stbi_img_data);
	return std::nullopt;
}

Image::Image(const uint32_t width, const uint32_t height, const PixelType* const data)
	: size_{width, height}, data_(data, data + width * height)
{}

Image::Image(const uint32_t width, const uint32_t height, std::vector<PixelType> data)
	: size_{width, height}, data_(std::move(data))
{}

uint32_t Image::GetWidth() const
{
	return size_[0];
}

uint32_t Image::GetHeight() const
{
	return size_[1];

}
const Image::PixelType* Image::GetData() const
{
	return data_.data();
}

} // namespace KK
