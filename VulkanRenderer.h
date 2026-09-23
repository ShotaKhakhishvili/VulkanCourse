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

	VkInstance instance = VK_NULL_HANDLE;

	struct {
		VkPhysicalDevice physicaDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue;

	// create functions

	void createInstance();
	void createLogicalDevice();

	// getter functions

	void getPhysicalDevice();
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice physDevice);

	// checker functions

	bool checkInstanceExtensionSupport(std::vector<const char*>* extensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);

};
