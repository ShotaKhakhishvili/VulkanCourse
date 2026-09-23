#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanRenderer.h"

#include <iostream>

GLFWwindow* window;

void initWindow(std::string name, uint32_t width, uint32_t height)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
}

int main()
{
	initWindow("Vulkan Window", 800, 600);
	
	VulkanRenderer vulkanRenderer;
	if (vulkanRenderer.init(window) != EXIT_SUCCESS)
	{
		return EXIT_FAILURE;
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	vulkanRenderer.cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}