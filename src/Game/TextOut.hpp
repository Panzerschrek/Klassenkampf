#pragma once
#include "GPUDataUploader.hpp"
#include "WindowVulkan.hpp"
#include <string_view>


// Hack! Shitty windows.h defines it
#ifdef TextOut
#undef TextOut
#endif

namespace KK
{


class TextOut final
{
public:
	TextOut(
		WindowVulkan& window_vulkan,
		GPUDataUploader& gpu_data_uploader);
	~TextOut();

	float GetMaxRows() const;

	void AddText(
		float column,
		float row,
		float size/*in letters*/,
		const uint8_t* color,
		std::string_view text);
	void AddTextPixelCoords(
		float x,
		float y,
		float size/*in pixels*/,
		const uint8_t* color,
		std::string_view text);

	void BeginFrame(vk::CommandBuffer command_buffer);
	void EndFrame(vk::CommandBuffer command_buffer);

private:
	struct TextVertex
	{
		float pos[2];
		uint8_t color[4];
		uint8_t tex_coord[2];
		uint8_t reserved[2];
	};
	static_assert(sizeof(TextVertex) == 16u, "Invalid size");

private:
	const vk::Device vk_device_;
	const vk::Extent2D viewport_size_;
	GPUDataUploader& gpu_data_uploader_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_frag_;

	vk::UniqueSampler font_image_sampler_;
	vk::UniqueDescriptorSetLayout descriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueImage font_image_;
	vk::UniqueDeviceMemory font_image_memory_;
	vk::UniqueImageView font_image_view_;

	size_t max_glyphs_in_buffer_= 0u;
	vk::UniqueBuffer vertex_buffer_;
	vk::UniqueDeviceMemory vertex_buffer_memory_;

	vk::UniqueBuffer index_buffer_;
	vk::UniqueDeviceMemory index_buffer_memory_;

	vk::UniqueDescriptorPool descriptor_pool_;
	vk::UniqueDescriptorSet descriptor_set_;

	std::vector<TextVertex> vertices_data_;
	uint32_t glyph_size_[2]={};
};

} // namespace KK
