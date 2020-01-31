#pragma once
#include "WindowVulkan.hpp"


namespace KK
{

class GPUDataUploader final
{
public:
	explicit GPUDataUploader(WindowVulkan& window_vulkan);
	~GPUDataUploader();

	struct RequestResult
	{
		vk::Buffer buffer;
		size_t buffer_offset;
		void* buffer_data;
		vk::CommandBuffer command_buffer;
	};

	RequestResult RequestMemory(size_t size);

	void Flush();

private:
	struct DataBuffer
	{
		vk::UniqueCommandBuffer command_buffer;
		bool command_buffer_active= false;
		vk::UniqueFence fence;
		vk::UniqueBuffer data_buffer;
		vk::UniqueDeviceMemory data_buffer_memory;
		void* data_mapped= nullptr;
	};

private:
	const vk::Device vk_device_;
	const vk::Queue queue_;

	vk::UniqueCommandPool command_pool_;

	const size_t buffer_data_size_= 2048u * 2048u * 4u;
	std::vector<DataBuffer> data_buffer_queue_;

	size_t current_data_buffer_index_= 0u;
	size_t current_data_buffer_offset_= 0u;
};

} // namespacek
