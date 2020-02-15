#include "GPUDataUploader.hpp"
#include "Assert.hpp"


namespace KK
{

GPUDataUploader::GPUDataUploader(WindowVulkan& window_vulkan)
	: vk_device_(window_vulkan.GetVulkanDevice())
	, queue_(window_vulkan.GetQueue())
{
	const vk::PhysicalDeviceMemoryProperties& memory_properties= window_vulkan.GetMemoryProperties();

	command_pool_=
		vk_device_.createCommandPoolUnique(
			vk::CommandPoolCreateInfo(
				vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				window_vulkan.GetQueueFamilyIndex()));

	data_buffer_queue_.resize(4u);
	for(DataBuffer& data_buffer : data_buffer_queue_)
	{
		data_buffer.command_buffer=
				std::move(
				vk_device_.allocateCommandBuffersUnique(
					vk::CommandBufferAllocateInfo(
						*command_pool_,
						vk::CommandBufferLevel::ePrimary,
						1u)).front());

		data_buffer.fence= vk_device_.createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));

		data_buffer.data_buffer=
			vk_device_.createBufferUnique(
				vk::BufferCreateInfo(
					vk::BufferCreateFlags(),
					buffer_data_size_,
					vk::BufferUsageFlagBits::eTransferSrc));

		const vk::MemoryRequirements memory_requirements= vk_device_.getBufferMemoryRequirements(*data_buffer.data_buffer);

		vk::MemoryAllocateInfo buffer_allocate_info(memory_requirements.size);
		for(uint32_t i= 0u; i < memory_properties.memoryTypeCount; ++i)
		{
			if((memory_requirements.memoryTypeBits & (1u << i)) != 0 &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible ) != vk::MemoryPropertyFlags() &&
				(memory_properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) != vk::MemoryPropertyFlags())
				buffer_allocate_info.memoryTypeIndex= i;
		}

		data_buffer.data_buffer_memory= vk_device_.allocateMemoryUnique(buffer_allocate_info);
		vk_device_.bindBufferMemory(*data_buffer.data_buffer, *data_buffer.data_buffer_memory, 0u);

		vk_device_.mapMemory(*data_buffer.data_buffer_memory, 0u, buffer_data_size_, vk::MemoryMapFlags(), &data_buffer.data_mapped);
	}
}

GPUDataUploader::~GPUDataUploader()
{
	Flush();

	// Unmap memory.
	for(const DataBuffer& data_buffer : data_buffer_queue_)
	{
		vk_device_.unmapMemory(*data_buffer.data_buffer_memory);
	}

	// Sync before destruction.
	vk_device_.waitIdle();
}

GPUDataUploader::RequestResult GPUDataUploader::RequestMemory(size_t size)
{
	size= (size + 15u) & ~15u; // Save alignment.

	KK_ASSERT(size <= buffer_data_size_);
	if(current_data_buffer_offset_ + size > buffer_data_size_)
	{
		Flush();
	}

	DataBuffer& current_buffer= data_buffer_queue_[current_data_buffer_index_];

	if(!current_buffer.command_buffer_active)
	{
		current_buffer.command_buffer_active= true;

		vk_device_.waitForFences(
			1u, &*current_buffer.fence,
			VK_TRUE,
			std::numeric_limits<uint64_t>::max());

		vk_device_.resetFences(1u, &*current_buffer.fence);

		current_buffer.command_buffer->begin(
			vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	}

	RequestResult result;
	result.command_buffer= *current_buffer.command_buffer;
	result.buffer_data= current_buffer.data_mapped;
	result.buffer_offset= current_data_buffer_offset_;
	result.buffer= *current_buffer.data_buffer;

	current_data_buffer_offset_+= size;
	KK_ASSERT(current_data_buffer_offset_ <= buffer_data_size_);

	return result;
}

size_t GPUDataUploader::GetMaxMemoryBlockSize() const
{
	return buffer_data_size_;
}

void GPUDataUploader::Flush()
{
	DataBuffer& current_buffer= data_buffer_queue_[current_data_buffer_index_];

	if(!current_buffer.command_buffer_active)
		return;
	current_buffer.command_buffer_active= false;

	current_buffer.command_buffer->end();

	const vk::PipelineStageFlags wait_dst_stage_mask= vk::PipelineStageFlagBits::eColorAttachmentOutput; // TODO - select correct.
	const vk::SubmitInfo vk_submit_info(
		0u, nullptr,
		&wait_dst_stage_mask,
		1u, &*current_buffer.command_buffer,
		0u, nullptr);
	queue_.submit(vk_submit_info, *current_buffer.fence);

	++current_data_buffer_index_;
	if(current_data_buffer_index_ == data_buffer_queue_.size())
		current_data_buffer_index_= 0u;
	current_data_buffer_offset_= 0u;

}

} // namespace KK
