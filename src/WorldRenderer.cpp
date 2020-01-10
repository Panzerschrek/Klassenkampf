#include "WorldRenderer.hpp"
#include <cmath>
#include <cstring>


namespace KK
{

namespace
{

namespace Shaders
{

#include "shaders/triangle.vert.sprv.h"
#include "shaders/triangle.frag.sprv.h"

} // namespace Shaders

struct WorldVertex
{
	float pos[3];
};

} // namespace

WorldRenderer::WorldRenderer(
	const VkDevice vk_device,
	const VkFormat surface_format,
	const VkExtent2D viewport_size,
	const VkPhysicalDeviceMemoryProperties& memory_properties,
	const std::vector<VkImageView>& swapchain_image_views)
	: vk_device_(vk_device)
	, viewport_size_(viewport_size)
{
	VkAttachmentDescription vk_attachment_description;
	std::memset(&vk_attachment_description, 0, sizeof(vk_attachment_description));
	vk_attachment_description.flags= 0u;
	vk_attachment_description.format= surface_format;
	vk_attachment_description.samples= VK_SAMPLE_COUNT_1_BIT; // TODO - maybe use 0?
	vk_attachment_description.loadOp= VK_ATTACHMENT_LOAD_OP_CLEAR;
	vk_attachment_description.storeOp= VK_ATTACHMENT_STORE_OP_STORE;
	vk_attachment_description.stencilLoadOp= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	vk_attachment_description.stencilStoreOp= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	vk_attachment_description.initialLayout= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vk_attachment_description.finalLayout= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference vk_attachment_reference;
	std::memset(&vk_attachment_reference, 0, sizeof(vk_attachment_reference));
	vk_attachment_reference.attachment= 0u;
	vk_attachment_reference.layout= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription vk_subpass_description;
	std::memset(&vk_subpass_description, 0, sizeof(vk_subpass_description));
	vk_subpass_description.flags= 0u;
	vk_subpass_description.pipelineBindPoint= VK_PIPELINE_BIND_POINT_GRAPHICS;
	vk_subpass_description.inputAttachmentCount= 0u;
	vk_subpass_description.pInputAttachments= nullptr;
	vk_subpass_description.colorAttachmentCount= 1u;
	vk_subpass_description.pColorAttachments= &vk_attachment_reference;
	vk_subpass_description.pResolveAttachments= nullptr;
	vk_subpass_description.pDepthStencilAttachment= nullptr;
	vk_subpass_description.preserveAttachmentCount= 0u;
	vk_subpass_description.pPreserveAttachments= nullptr;

	VkRenderPassCreateInfo vk_render_pass_create_info;
	std::memset(&vk_render_pass_create_info, 0, sizeof(vk_render_pass_create_info));
	vk_render_pass_create_info.sType= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	vk_render_pass_create_info.pNext= nullptr;
	vk_render_pass_create_info.flags= 0u;
	vk_render_pass_create_info.attachmentCount= 1u;
	vk_render_pass_create_info.pAttachments= &vk_attachment_description;
	vk_render_pass_create_info.subpassCount= 1u;
	vk_render_pass_create_info.pSubpasses= &vk_subpass_description;

	vkCreateRenderPass(vk_device_, &vk_render_pass_create_info, nullptr, &vk_render_pass_);

	vk_framebuffers_.resize(swapchain_image_views.size());
	for(size_t i= 0u; i < swapchain_image_views.size(); ++i)
	{
		VkFramebufferCreateInfo vk_framebuffer_create_info;
		std::memset(&vk_framebuffer_create_info, 0, sizeof(vk_framebuffer_create_info));
		vk_framebuffer_create_info.sType= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		vk_framebuffer_create_info.pNext= nullptr;
		vk_framebuffer_create_info.flags= 0u;
		vk_framebuffer_create_info.renderPass= vk_render_pass_;
		vk_framebuffer_create_info.attachmentCount= 1u;
		vk_framebuffer_create_info.pAttachments= &swapchain_image_views[i];
		vk_framebuffer_create_info.width = viewport_size_.width ;
		vk_framebuffer_create_info.height= viewport_size_.height;
		vk_framebuffer_create_info.layers= 1u;

		vkCreateFramebuffer(vk_device_, &vk_framebuffer_create_info, nullptr, &vk_framebuffers_[i]);
	}

	// Create shaders

	VkShaderModuleCreateInfo vk_shader_module_create_info;
	std::memset(&vk_shader_module_create_info, 0, sizeof(vk_shader_module_create_info));
	vk_shader_module_create_info.sType= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vk_shader_module_create_info.pNext= nullptr;
	vk_shader_module_create_info.flags= 0u;

	vk_shader_module_create_info.pCode= reinterpret_cast<const uint32_t*>(Shaders::c_triangle_vert_file_content);
	vk_shader_module_create_info.codeSize= std::size(Shaders::c_triangle_vert_file_content);
	vkCreateShaderModule(vk_device_, &vk_shader_module_create_info, nullptr, &shader_vert_);

	vk_shader_module_create_info.pCode= reinterpret_cast<const uint32_t*>(Shaders::c_triangle_frag_file_content);
	vk_shader_module_create_info.codeSize= std::size(Shaders::c_triangle_frag_file_content);
	vkCreateShaderModule(vk_device_, &vk_shader_module_create_info, nullptr, &shader_frag_);

	// Create pipeline layout

	VkDescriptorSetLayoutBinding vk_descriptor_set_layout_binding;
	std::memset(&vk_descriptor_set_layout_binding, 0, sizeof(vk_descriptor_set_layout_binding));
	vk_descriptor_set_layout_binding.binding= 0u;
	vk_descriptor_set_layout_binding.descriptorType= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vk_descriptor_set_layout_binding.descriptorCount= 1u;
	vk_descriptor_set_layout_binding.stageFlags= VK_SHADER_STAGE_VERTEX_BIT;
	vk_descriptor_set_layout_binding.pImmutableSamplers= nullptr;

	VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_create_info;
	std::memset(&vk_descriptor_set_layout_create_info, 0, sizeof(vk_descriptor_set_layout_create_info));
	vk_descriptor_set_layout_create_info.sType= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	vk_descriptor_set_layout_create_info.pNext= nullptr;
	vk_descriptor_set_layout_create_info.bindingCount= 1u;
	vk_descriptor_set_layout_create_info.pBindings = &vk_descriptor_set_layout_binding;

	vkCreateDescriptorSetLayout(vk_device_, &vk_descriptor_set_layout_create_info, nullptr, &vk_decriptor_set_layout_);

	VkPushConstantRange vk_push_constant_range;
	std::memset(&vk_push_constant_range, 0, sizeof(vk_push_constant_range));
	vk_push_constant_range.stageFlags= VK_SHADER_STAGE_VERTEX_BIT;
	vk_push_constant_range.offset= 0u;
	vk_push_constant_range.size= sizeof(float) * 2;

	VkPipelineLayoutCreateInfo vk_pipeline_layout_create_info;
	std::memset(&vk_pipeline_layout_create_info, 0, sizeof(vk_pipeline_layout_create_info));
	vk_pipeline_layout_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vk_pipeline_layout_create_info.pNext= nullptr;
	vk_pipeline_layout_create_info.setLayoutCount= 1u;
	vk_pipeline_layout_create_info.pSetLayouts= &vk_decriptor_set_layout_;
	vk_pipeline_layout_create_info.pushConstantRangeCount= 1u;
	vk_pipeline_layout_create_info.pPushConstantRanges= &vk_push_constant_range;

	vkCreatePipelineLayout(vk_device_, &vk_pipeline_layout_create_info, nullptr, &vk_pipeline_layout_);

	// Create pipeline.

	VkPipelineShaderStageCreateInfo vk_shader_stage_create_info[2];
	std::memset(vk_shader_stage_create_info, 0, sizeof(vk_shader_stage_create_info));
	for(VkPipelineShaderStageCreateInfo& info : vk_shader_stage_create_info)
	{
		info.sType= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext= nullptr;
		info.flags= 0;
	}

	vk_shader_stage_create_info[0].stage= VK_SHADER_STAGE_VERTEX_BIT;
	vk_shader_stage_create_info[0].module= shader_vert_;
	vk_shader_stage_create_info[0].pName= "main";
	vk_shader_stage_create_info[1].stage= VK_SHADER_STAGE_FRAGMENT_BIT;
	vk_shader_stage_create_info[1].module= shader_frag_;
	vk_shader_stage_create_info[1].pName= "main";

	VkVertexInputBindingDescription vk_vertex_input_binding_description;
	std::memset(&vk_vertex_input_binding_description, 0, sizeof(vk_vertex_input_binding_description));
	vk_vertex_input_binding_description.binding= 0u;
	vk_vertex_input_binding_description.inputRate= VK_VERTEX_INPUT_RATE_VERTEX;
	vk_vertex_input_binding_description.stride= sizeof(WorldVertex);

	VkVertexInputAttributeDescription vk_vertex_input_attribute_description;
	std::memset(&vk_vertex_input_attribute_description, 0, sizeof(vk_vertex_input_attribute_description));
	vk_vertex_input_attribute_description.location= 0u;
	vk_vertex_input_attribute_description.binding= 0u;
	vk_vertex_input_attribute_description.format= VK_FORMAT_R32G32B32_SFLOAT;
	vk_vertex_input_attribute_description.offset= 0u;

	VkPipelineVertexInputStateCreateInfo vk_pipiline_vertex_input_state_create_info;
	std::memset(&vk_pipiline_vertex_input_state_create_info, 0, sizeof(vk_pipiline_vertex_input_state_create_info));
	vk_pipiline_vertex_input_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vk_pipiline_vertex_input_state_create_info.pNext= nullptr;
	vk_pipiline_vertex_input_state_create_info.flags= 0u;
	vk_pipiline_vertex_input_state_create_info.vertexBindingDescriptionCount= 1u;
	vk_pipiline_vertex_input_state_create_info.pVertexBindingDescriptions= &vk_vertex_input_binding_description;
	vk_pipiline_vertex_input_state_create_info.vertexAttributeDescriptionCount= 1u;
	vk_pipiline_vertex_input_state_create_info.pVertexAttributeDescriptions= &vk_vertex_input_attribute_description;

	VkPipelineTessellationStateCreateInfo vk_pipeline_tesselation_state_create_info;
	std::memset(&vk_pipeline_tesselation_state_create_info, 0, sizeof(vk_pipeline_tesselation_state_create_info));
	vk_pipeline_tesselation_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	vk_pipeline_tesselation_state_create_info.pNext= nullptr;
	vk_pipeline_tesselation_state_create_info.flags= 0u;
	vk_pipeline_tesselation_state_create_info.patchControlPoints= 0u;

	VkPipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info;
	std::memset(&vk_pipeline_input_assembly_state_create_info, 0, sizeof(vk_pipeline_input_assembly_state_create_info));
	vk_pipeline_input_assembly_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	vk_pipeline_input_assembly_state_create_info.pNext= nullptr;
	vk_pipeline_input_assembly_state_create_info.flags= 0u;
	vk_pipeline_input_assembly_state_create_info.topology= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	vk_pipeline_input_assembly_state_create_info.primitiveRestartEnable= VK_FALSE;

	VkViewport vk_viewport;
	std::memset(&vk_viewport, 0, sizeof(vk_viewport));
	vk_viewport.x= 0.0f;
	vk_viewport.y= 0.0f;
	vk_viewport.width = float(viewport_size_.width );
	vk_viewport.height= float(viewport_size_.height);
	vk_viewport.minDepth= 0.0f;
	vk_viewport.maxDepth= 1.0f;

	VkRect2D vk_scissor;
	std::memset(&vk_scissor, 0, sizeof(vk_scissor));
	vk_scissor.offset.x= 0;
	vk_scissor.offset.y= 0;
	vk_scissor.extent= viewport_size_;

	VkPipelineViewportStateCreateInfo vk_pipieline_viewport_state_create_info;
	std::memset(&vk_pipieline_viewport_state_create_info, 0, sizeof(vk_pipieline_viewport_state_create_info));
	vk_pipieline_viewport_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vk_pipieline_viewport_state_create_info.pNext= nullptr;
	vk_pipieline_viewport_state_create_info.flags= 0u;
	vk_pipieline_viewport_state_create_info.viewportCount= 1u;
	vk_pipieline_viewport_state_create_info.pViewports= &vk_viewport;
	vk_pipieline_viewport_state_create_info.scissorCount= 1u;
	vk_pipieline_viewport_state_create_info.pScissors= &vk_scissor;

	VkPipelineRasterizationStateCreateInfo vk_pipilane_rasterization_state_create_info;
	std::memset(&vk_pipilane_rasterization_state_create_info, 0, sizeof(vk_pipilane_rasterization_state_create_info));
	vk_pipilane_rasterization_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	vk_pipilane_rasterization_state_create_info.pNext= nullptr;
	vk_pipilane_rasterization_state_create_info.flags= 0u;
	vk_pipilane_rasterization_state_create_info.depthClampEnable= VK_FALSE;
	vk_pipilane_rasterization_state_create_info.rasterizerDiscardEnable= VK_FALSE;
	vk_pipilane_rasterization_state_create_info.polygonMode= VK_POLYGON_MODE_FILL;
	vk_pipilane_rasterization_state_create_info.cullMode= VK_CULL_MODE_NONE;
	vk_pipilane_rasterization_state_create_info.frontFace= VK_FRONT_FACE_COUNTER_CLOCKWISE;
	vk_pipilane_rasterization_state_create_info.depthBiasEnable= VK_FALSE;
	vk_pipilane_rasterization_state_create_info.depthBiasConstantFactor= 0.0f;
	vk_pipilane_rasterization_state_create_info.depthBiasClamp= 0.0f;
	vk_pipilane_rasterization_state_create_info.depthBiasSlopeFactor= 0.0f;
	vk_pipilane_rasterization_state_create_info.lineWidth= 1.0f;

	const uint32_t sample_mask= ~0u;
	VkPipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info;
	std::memset(&vk_pipeline_multisample_state_create_info, 0, sizeof(vk_pipeline_multisample_state_create_info));
	vk_pipeline_multisample_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	vk_pipeline_multisample_state_create_info.pNext= nullptr;
	vk_pipeline_multisample_state_create_info.flags= 0u;
	vk_pipeline_multisample_state_create_info.rasterizationSamples= VK_SAMPLE_COUNT_1_BIT;
	vk_pipeline_multisample_state_create_info.sampleShadingEnable= VK_FALSE;
	vk_pipeline_multisample_state_create_info.minSampleShading= 1.0f;
	vk_pipeline_multisample_state_create_info.pSampleMask= &sample_mask;
	vk_pipeline_multisample_state_create_info.alphaToCoverageEnable= VK_FALSE;
	vk_pipeline_multisample_state_create_info.alphaToOneEnable= VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo vk_pipeline_depth_stencil_state_create_info;
	std::memset(&vk_pipeline_depth_stencil_state_create_info, 0, sizeof(vk_pipeline_depth_stencil_state_create_info));
	vk_pipeline_depth_stencil_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	vk_pipeline_depth_stencil_state_create_info.pNext= nullptr;
	vk_pipeline_depth_stencil_state_create_info.flags= 0u;
	vk_pipeline_depth_stencil_state_create_info.depthTestEnable= VK_FALSE;
	vk_pipeline_depth_stencil_state_create_info.depthWriteEnable= VK_FALSE;
	vk_pipeline_depth_stencil_state_create_info.depthCompareOp= VK_COMPARE_OP_NEVER;
	vk_pipeline_depth_stencil_state_create_info.depthBoundsTestEnable= VK_FALSE;
	vk_pipeline_depth_stencil_state_create_info.stencilTestEnable= VK_FALSE;
	//vk_pipeline_depth_stencil_state_create_info.front; // TODO
	//vk_pipeline_depth_stencil_state_create_info.back; // TOOD
	vk_pipeline_depth_stencil_state_create_info.minDepthBounds= -1.0f;
	vk_pipeline_depth_stencil_state_create_info.maxDepthBounds= 1.0f;

	VkPipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state;
	std::memset(&vk_pipeline_color_blend_attachment_state, 0, sizeof(vk_pipeline_color_blend_attachment_state));
	vk_pipeline_color_blend_attachment_state.blendEnable= VK_FALSE;
	vk_pipeline_color_blend_attachment_state.srcColorBlendFactor= VK_BLEND_FACTOR_ONE;
	vk_pipeline_color_blend_attachment_state.dstColorBlendFactor= VK_BLEND_FACTOR_ZERO;
	vk_pipeline_color_blend_attachment_state.colorBlendOp= VK_BLEND_OP_ADD;
	vk_pipeline_color_blend_attachment_state.srcAlphaBlendFactor= VK_BLEND_FACTOR_ONE;
	vk_pipeline_color_blend_attachment_state.dstAlphaBlendFactor= VK_BLEND_FACTOR_ZERO;
	vk_pipeline_color_blend_attachment_state.alphaBlendOp= VK_BLEND_OP_ADD;
	vk_pipeline_color_blend_attachment_state.colorWriteMask= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info;
	std::memset(&vk_pipeline_color_blend_state_create_info, 0, sizeof(vk_pipeline_color_blend_state_create_info));
	vk_pipeline_color_blend_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	vk_pipeline_color_blend_state_create_info.pNext= nullptr;
	vk_pipeline_color_blend_state_create_info.flags= 0;
	vk_pipeline_color_blend_state_create_info.logicOpEnable= VK_FALSE;
	vk_pipeline_color_blend_state_create_info.logicOp= VK_LOGIC_OP_COPY;
	vk_pipeline_color_blend_state_create_info.attachmentCount= 1u;
	vk_pipeline_color_blend_state_create_info.pAttachments= &vk_pipeline_color_blend_attachment_state;
	std::memset(vk_pipeline_color_blend_state_create_info.blendConstants, 0, sizeof(float)*4);

	VkPipelineDynamicStateCreateInfo vk_pipeline_dynamic_state_create_info;
	std::memset(&vk_pipeline_dynamic_state_create_info, 0, sizeof(vk_pipeline_dynamic_state_create_info));
	vk_pipeline_dynamic_state_create_info.sType= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	vk_pipeline_dynamic_state_create_info.pNext= nullptr;
	vk_pipeline_dynamic_state_create_info.flags= 0u;
	vk_pipeline_dynamic_state_create_info.dynamicStateCount= 0u;
	vk_pipeline_dynamic_state_create_info.pDynamicStates= nullptr;

	VkGraphicsPipelineCreateInfo vk_graphics_pipeline_create_info;
	std::memset(&vk_graphics_pipeline_create_info, 0, sizeof(vk_graphics_pipeline_create_info));
	vk_graphics_pipeline_create_info.sType= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	vk_graphics_pipeline_create_info.pNext= nullptr;
	vk_graphics_pipeline_create_info.flags= 0u;
	vk_graphics_pipeline_create_info.stageCount= uint32_t(std::size(vk_shader_stage_create_info));
	vk_graphics_pipeline_create_info.pStages= vk_shader_stage_create_info;
	vk_graphics_pipeline_create_info.pVertexInputState= &vk_pipiline_vertex_input_state_create_info;
	vk_graphics_pipeline_create_info.pInputAssemblyState= &vk_pipeline_input_assembly_state_create_info;
	//vk_graphics_pipeline_create_info.pTessellationState= &vk_pipeline_tesselation_state_create_info;
	vk_graphics_pipeline_create_info.pViewportState= &vk_pipieline_viewport_state_create_info;
	vk_graphics_pipeline_create_info.pRasterizationState= &vk_pipilane_rasterization_state_create_info;
	vk_graphics_pipeline_create_info.pMultisampleState= &vk_pipeline_multisample_state_create_info;
	//vk_graphics_pipeline_create_info.pDepthStencilState= &vk_pipeline_depth_stencil_state_create_info;
	vk_graphics_pipeline_create_info.pColorBlendState= &vk_pipeline_color_blend_state_create_info;
	//vk_graphics_pipeline_create_info.pDynamicState= &vk_pipeline_dynamic_state_create_info;
	vk_graphics_pipeline_create_info.layout= vk_pipeline_layout_;
	vk_graphics_pipeline_create_info.renderPass= vk_render_pass_;
	vk_graphics_pipeline_create_info.subpass= 0u;
	vk_graphics_pipeline_create_info.basePipelineHandle= nullptr;
	vk_graphics_pipeline_create_info.basePipelineIndex= ~0u;

	vkCreateGraphicsPipelines(vk_device_, nullptr, 1u, &vk_graphics_pipeline_create_info, nullptr, &vk_pipeline_);

	// Create vertex buffer

	std::vector<WorldVertex> world_vertices
	{
		{ -0.5f, -0.5f, 0.0f },
		{ +0.5f, -0.5f, 0.0f },
		{ +0.5f, +0.5f, 0.0f },
	};

	VkBufferCreateInfo vk_vertex_buffer_create_info;
	std::memset(&vk_vertex_buffer_create_info, 0, sizeof(vk_vertex_buffer_create_info));
	vk_vertex_buffer_create_info.sType= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vk_vertex_buffer_create_info.pNext= nullptr;
	vk_vertex_buffer_create_info.size= world_vertices.size() * sizeof(WorldVertex);
	vk_vertex_buffer_create_info.usage= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vk_vertex_buffer_create_info.sharingMode= VK_SHARING_MODE_EXCLUSIVE;
	vk_vertex_buffer_create_info.queueFamilyIndexCount= 0u;
	vk_vertex_buffer_create_info.pQueueFamilyIndices= nullptr;

	vkCreateBuffer(vk_device_, &vk_vertex_buffer_create_info, nullptr, &vk_vertex_buffer_);

	VkMemoryRequirements vertex_buffer_memory_requirements;
	vkGetBufferMemoryRequirements(vk_device_, vk_vertex_buffer_, &vertex_buffer_memory_requirements);

	VkMemoryAllocateInfo vk_memory_allocate_info;
	std::memset(&vk_memory_allocate_info, 0, sizeof(vk_memory_allocate_info));
	vk_memory_allocate_info.sType= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vk_memory_allocate_info.pNext= nullptr;
	vk_memory_allocate_info.allocationSize= vertex_buffer_memory_requirements.size;

	for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
	{
		if((vertex_buffer_memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0)
			vk_memory_allocate_info.memoryTypeIndex= i;
	}

	vkAllocateMemory(vk_device_, &vk_memory_allocate_info, nullptr, &vk_vertex_buffer_memory_);

	vkBindBufferMemory(vk_device_, vk_vertex_buffer_, vk_vertex_buffer_memory_, 0u);

	void* vertex_data_gpu_size= nullptr;
	vkMapMemory(vk_device_, vk_vertex_buffer_memory_, 0u, vk_memory_allocate_info.allocationSize, 0, &vertex_data_gpu_size);
	std::memcpy(vertex_data_gpu_size, world_vertices.data(), world_vertices.size() * sizeof(WorldVertex));
	vkUnmapMemory(vk_device_, vk_vertex_buffer_memory_);
}

WorldRenderer::~WorldRenderer()
{
	vkFreeMemory(vk_device_, vk_vertex_buffer_memory_, nullptr);
	vkDestroyBuffer(vk_device_, vk_vertex_buffer_, nullptr);

	vkDestroyPipeline(vk_device_, vk_pipeline_, nullptr);

	vkDestroyDescriptorSetLayout(vk_device_, vk_decriptor_set_layout_, nullptr);
	vkDestroyPipelineLayout(vk_device_, vk_pipeline_layout_, nullptr);

	vkDestroyShaderModule(vk_device_, shader_vert_, nullptr);
	vkDestroyShaderModule(vk_device_, shader_frag_, nullptr);

	for(const VkFramebuffer framebuffer : vk_framebuffers_)
	{
		vkDestroyFramebuffer(vk_device_, framebuffer, nullptr);
	}

	vkDestroyRenderPass(vk_device_, vk_render_pass_, nullptr);
}

void WorldRenderer::Draw(
	const VkCommandBuffer command_buffer,
	const size_t swapchain_image_index,
	const float frame_time_s)
{
	(void)command_buffer;

	VkClearValue clear_value;
	std::memset(&clear_value, 0, sizeof(clear_value));
	clear_value.color.float32[0]= 0.25f;
	clear_value.color.float32[1]= 1.0f;
	clear_value.color.float32[2]= 0.25f;
	clear_value.color.float32[3]= 0.5f;

	VkRenderPassBeginInfo render_pass_begin_info;
	std::memset(&render_pass_begin_info, 0, sizeof(render_pass_begin_info));
	render_pass_begin_info.sType= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext= nullptr;
	render_pass_begin_info.renderPass= vk_render_pass_;
	render_pass_begin_info.framebuffer= vk_framebuffers_[swapchain_image_index];
	render_pass_begin_info.renderArea.offset.x= 0u;
	render_pass_begin_info.renderArea.offset.y= 0u;
	render_pass_begin_info.renderArea.extent= viewport_size_;
	render_pass_begin_info.clearValueCount= 1u;
	render_pass_begin_info.pClearValues= &clear_value;

	vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	{
		const VkDeviceSize offsets= 0u;
		vkCmdBindVertexBuffers(command_buffer, 0u, 1u, &vk_vertex_buffer_, &offsets);

		float pos_delta[2]= { std::sin(frame_time_s) * 0.5f , std::cos(frame_time_s) * 0.5f };

		vkCmdPushConstants(command_buffer, vk_pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pos_delta), &pos_delta);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_);
		vkCmdDraw(command_buffer, 3, 1, 0, 0);
	}
	vkCmdEndRenderPass(command_buffer);
}

} // namespace KK
