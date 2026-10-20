#pragma once

#include <fstream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECT_COUNT = 128;

const std::vector<const char*> deviceExtensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representaion
struct Vertex
{
	glm::vec3 pos;	        // Vertex position (x,y,z)
	glm::vec3 col;	        // Vertex color	(r,g,b)
    glm::vec2 tex;          // Texture coords     
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

static VkCommandBuffer beginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
    // Command buffer to hold transfer commands
    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    // Allocate command buffer from pool

    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    // Information to begin the command buffer record
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // we only use it once, so set up for one time use

    // Begin recording transfer commands
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void endAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
    // End the recording of the commands
    vkEndCommandBuffer(commandBuffer);

    // queue sumision info
    VkSubmitInfo submitInfo         = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &commandBuffer;

    // Submit transfer command to the transfer queue 
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

    // Wait for the queue
    vkQueueWaitIdle(queue);

    // Free the temp command buffer

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
    VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);
    // Region of data to copy from and to
    VkBufferCopy bufferCopyRegion   = {};
    bufferCopyRegion.srcOffset      = 0;
    bufferCopyRegion.dstOffset      = 0;
    bufferCopyRegion.size           = bufferSize;

    // Record command to copy from src buffer to dst command
    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
    VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer transferCommandBuffer = beginCommandBuffer(device, transferCommandPool);

    VkBufferImageCopy bufferImageCopyRegion = {};
    bufferImageCopyRegion.bufferOffset                      = 0;
    bufferImageCopyRegion.bufferRowLength                   = 0;
    bufferImageCopyRegion.bufferImageHeight                 = 0 ;
    bufferImageCopyRegion.imageSubresource.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopyRegion.imageSubresource.mipLevel         = 0;
    bufferImageCopyRegion.imageSubresource.baseArrayLayer   = 0;
    bufferImageCopyRegion.imageSubresource.layerCount       = 1;
    bufferImageCopyRegion.imageOffset                       = { 0, 0, 0};
    bufferImageCopyRegion.imageExtent                       = { width, height, 1};

    vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopyRegion);

    endAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, 
    VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginCommandBuffer(device, commandPool);

    VkImageMemoryBarrier imageMemoryBarrier                 = {};
    imageMemoryBarrier.sType                                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout                            = oldLayout;
    imageMemoryBarrier.newLayout                            = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex                  = VK_QUEUE_FAMILY_IGNORED;        // Not set to IGNORED on default, requires explicit set
    imageMemoryBarrier.dstQueueFamilyIndex                  = VK_QUEUE_FAMILY_IGNORED;        // Not set to IGNORED on default, requires explicit set
    imageMemoryBarrier.image                                = image;
    imageMemoryBarrier.subresourceRange.aspectMask          = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel        = 0;
    imageMemoryBarrier.subresourceRange.levelCount          = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer      = 0;
    imageMemoryBarrier.subresourceRange.layerCount          = 1;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage, dstStage,
        0,
        0, nullptr,                 // Memory barrier count and data
        0, nullptr,                 // Buffer memory barrier count and data
        1, &imageMemoryBarrier      // Image memory barrier count and data
    );

    endAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
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