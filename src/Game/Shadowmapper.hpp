#pragma once
#include "../MathLib/Vec.hpp"
#include "ShadowmapSize.hpp"
#include "WindowVulkan.hpp"


namespace KK
{

class Shadowmapper final
{
public:
	Shadowmapper(
		WindowVulkan& window_vulkan,
		size_t vertex_size,
		size_t vertex_pos_offset,
		vk::Format vertex_pos_format);
	~Shadowmapper();

	ShadowmapSize GetSize() const;
	std::vector<vk::ImageView> GetDepthCubemapArrayImagesView() const;

	void DrawToDepthCubemap(
		vk::CommandBuffer command_buffer,
		ShadowmapSlot slot,
		const m_Vec3& light_pos,
		float light_radius,
		const std::function<void()>& draw_function);

private:
	struct Framebuffer
	{
		vk::UniqueFramebuffer framebuffer;
		vk::UniqueImageView image_view;
	};

	struct DetailLevel
	{
		uint32_t cubemap_size;
		uint32_t cubemap_count;
		vk::UniqueImage depth_cubemap_array_image;
		vk::UniqueDeviceMemory depth_cubemap_array_image_memory;
		vk::UniqueImageView depth_cubemap_array_image_view;
		std::vector<Framebuffer> framebuffers;
	};

private:
	const vk::Device vk_device_;

	vk::UniqueRenderPass render_pass_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_geom_;
	vk::UniqueShaderModule shader_frag_;

	vk::UniqueDescriptorSetLayout descriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueDescriptorPool descriptor_set_pool_;

	vk::UniqueBuffer uniforms_buffer_;
	vk::UniqueDeviceMemory uniforms_buffer_memory_;
	vk::UniqueDescriptorSet descriptor_set_;
	bool matrices_buffer_filled_= false;

	std::vector<DetailLevel> detail_levels_;
};

} // namespace KK
