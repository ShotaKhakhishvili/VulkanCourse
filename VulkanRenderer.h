#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

#include "Utilities.h"

class VulkanRenderer
{
public:

	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* newWindow);
	void cleanup();

private:
	GLFWwindow* window;

	// Main 

	VkInstance instance = VK_NULL_HANDLE;

	struct {
		VkPhysicalDevice physicalDevice	= VK_NULL_HANDLE;
		VkDevice logicalDevice			= VK_NULL_HANDLE;
	} mainDevice;

	VkQueue graphicsQueue;
	VkQueue presentationQueue;

	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	std::vector<SwapchainImage> swapchainImages;

	// Utilities

	VkFormat swapchainImageFormat;
	VkExtent2D swapchainImageExtent;

	// create functions

	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createSwapchain();

	// -------- Support Function -----------

	// getter functions

	void getPhysicalDevice();
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice physDevice);
	SwapchainDetails getSwapchainDetails(VkPhysicalDevice physDevice);

	// checker functions

	bool checkInstanceExtensionSupport(std::vector<const char*>* extensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	// choose functions

	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
	VkPresentModeKHR chooseBestPresentMode(const std::vector<VkPresentModeKHR>& capabilities);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	// create

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};
