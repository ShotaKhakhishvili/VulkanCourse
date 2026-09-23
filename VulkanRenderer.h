#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>

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

	void createInstance();
	bool checkInstanceExtensionSupport(std::vector<const char*>* extensions);
};
