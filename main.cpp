#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/trigonometric.hpp>
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

		angle += deltaTime;
		if (angle >= 360.f)
			angle -= 360.0f;

		glm::mat4 posShift0 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.3f, 6.0f));
		glm::mat4 posShift1 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -6.0f));

		//vulkanRenderer.updateModel(0, posShift0);
		vulkanRenderer.updateModel(0, glm::rotate(posShift0, glm::radians(angle * 40), glm::vec3(0.0f, 1.0f, 0.0f)));
		vulkanRenderer.updateModel(1, glm::rotate(posShift1, glm::radians(270.0f - angle * 40), glm::vec3(0.0f, 1.0f, 0.0f)));

		glfwPollEvents();
		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}