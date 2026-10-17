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
	initWindow("Vulkan Window", 1600, 900);
	
	VulkanRenderer vulkanRenderer;
	if (vulkanRenderer.init(window) != EXIT_SUCCESS)
	{
		return EXIT_FAILURE;
	}

	float angle = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	while (!glfwWindowShouldClose(window))
	{
		float now = glfwGetTime();
		deltaTime = lastTime - now;
		lastTime = now;

		angle += deltaTime * 10;
		if (angle >= 360.f)
			angle -= 360.0f;

		vulkanRenderer.updateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f)));

		glfwPollEvents();
		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}