#pragma once
#include "../MathLib/Vec.hpp"
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

	vk::ImageView GetDepthCubemapArrayImageView() const;

	void DrawToDepthCubemap(
		vk::CommandBuffer command_buffer,
		size_t cubemap_index,
		const m_Vec3& light_pos,
		const std::function<void()>& draw_function);

private:
	struct Framebuffer
	{
		vk::UniqueFramebuffer framebuffer;
		vk::UniqueImageView image_view;
	};

private:
	const vk::Device vk_device_;

	const uint32_t cubemap_size_;
	const uint32_t cubemap_count_;

	vk::UniqueImage depth_cubemap_array_image_;
	vk::UniqueDeviceMemory depth_cubemap_array_image_memory_;
	vk::UniqueImageView depth_cubemap_array_image_view_;

	vk::UniqueRenderPass render_pass_;

	vk::UniqueShaderModule shader_vert_;
	vk::UniqueShaderModule shader_geom_;
	vk::UniqueShaderModule shader_frag_;

	// Framebuffer for each cubemap
	std::vector<Framebuffer> framebuffers_;

	vk::UniqueDescriptorSetLayout descriptor_set_layout_;
	vk::UniquePipelineLayout pipeline_layout_;
	vk::UniquePipeline pipeline_;

	vk::UniqueDescriptorPool descriptor_set_pool_;
	vk::UniqueDescriptorSet descriptor_set_;

	vk::UniqueBuffer uniforms_buffer_;
	vk::UniqueDeviceMemory uniforms_buffer_memory_;
};

} // namespace KK
