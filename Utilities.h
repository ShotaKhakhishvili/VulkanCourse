#pragma once

#include <fstream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;

const std::vector<const char*> deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representaion
struct Vertex
{
	glm::vec3 pos;	// Vertex position (x,y,z)
	glm::vec3 col;	// Vertex color	(r,g,b)
};

struct QueueFamilyIndices
{
	int graphicsFamily = -1;
	int presentationFamily = -1;

	bool isValid()
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapchainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentationModes;
};

struct SwapchainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		throw std::runtime_error("Unable to open a file");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	file.seekg(0);
	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    // Get the properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        if ((allowedTypes & (1 << i))
            && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return 0;
}

static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags,
	VkMemoryPropertyFlags propertyFlags, VkBuffer* buffer, VkDeviceMemory* deviceMemory)
{

    // Create Buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to make a vertex buffer");
    }

    // Get buffer memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memRequirements.size;
    memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, propertyFlags);

    // Allocate Memory 

    result = vkAllocateMemory(device, &memoryAllocInfo, nullptr, deviceMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate vertex buffer memory!");
    }

    // Allocate memory to given vertexx buffer
    vkBindBufferMemory(device, *buffer, *deviceMemory, 0);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
    VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    // Command buffer to hold transfer commands
    VkCommandBuffer transferCommandBuffer;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool;
    allocInfo.commandBufferCount = 1;

    // Allocate command buffer from pool

    vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);

    // Information to begin the command buffer record
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // we only use it once, so set up for one time use

    // Begin recording transfer commands
    vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

    // Region of data to copy from and to
    VkBufferCopy bufferCopyRegion = {};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    // Record command to copy from src buffer to dst command
    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    // End the recording of the commands
    vkEndCommandBuffer(transferCommandBuffer);

    // queue sumision info
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCommandBuffer;

    // Submit transfer command to the transfer queue 
    vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);

    // Wait for the queue
    vkQueueWaitIdle(transferQueue);

    // Free the temp command buffer

    vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer); 
}