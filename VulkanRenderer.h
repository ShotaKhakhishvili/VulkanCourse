#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <vector>
#include <array>

#include "Utilities.h"
#include "Mesh.h"

class VulkanRenderer
{
public:

	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* newWindow);

	void updateModel(glm::mat4 newModel);

	void draw();
	void cleanup();

private:
	GLFWwindow* window;

	int currentFrame = 0;

	// Scene objects

	std::vector<Mesh> meshes;

	// Scene settings
	
	struct MVP
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	} mvp;

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
	std::vector<VkFramebuffer> swapchainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	// Descriptorss

	VkDescriptorSetLayout descriptorSetLayout;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkBuffer> uniformBuffer;
	std::vector<VkDeviceMemory> uniformBufferMemory;

	// Pipeline
	
	VkPipeline	graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	// Pools

	VkCommandPool graphicsCommandPool;

	// Utilities

	VkFormat swapchainImageFormat;
	VkExtent2D swapchainImageExtent;

	// Synchronization

	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

	// create functions

	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createSwapchain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronization();

	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	void updateUniformBuffer(uint32_t imageIndex);

	// record functinos

	void recordCommands();

	// -------- Support Function 

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
	VkShaderModule createShaderModule(const std::vector<char>& fileBuffer);

	// create

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};
