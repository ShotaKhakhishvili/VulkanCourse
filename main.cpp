#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>

#include <iostream>

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* window = glfwCreateWindow(800, 600, "Test Window", nullptr, nullptr);

	uint32_t extensionsCount;

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);

	glm::mat4 testMat{ 0.0f };
	glm::vec4 testMat1{ 0.0f };

	auto res = testMat * testMat1;

	std::cout << extensionsCount << std::endl;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}