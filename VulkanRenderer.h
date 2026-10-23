#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include <stdexcept>
#include <vector>
#include <array>

#include "Utilities.h"
#include "MeshModel.h"

class VulkanRenderer
{
public:

	VulkanRenderer();
	~VulkanRenderer();

	int init(GLFWwindow* newWindow);

	void updateModel(size_t meshIndex, glm::mat4 newModel);

	void draw();
	void cleanup();

private:
	GLFWwindow* window;

	int currentFrame = 0;

	// Scene settings
	
	struct UboViewProjection
	{
		glm::mat4 view;
		glm::mat4 projection;
	} uboViewProjection;

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

	std::vector<VkImage> colorBufferImage;
	std::vector<VkDeviceMemory> colorBufferImageMemory;
	std::vector<VkImageView> colorBufferImageView;
	VkFormat colorFormat;

	std::vector<VkImage> depthBufferImage;
	std::vector<VkDeviceMemory> depthBufferImageMemory;
	std::vector<VkImageView> depthBufferImageView;
	VkFormat depthFormat;

	VkSampler textureSampler;

	// Descriptors

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout inputSetLayout;
	VkDescriptorSetLayout samplerSetLayout;
	VkPushConstantRange pushConstantRange;

	VkDescriptorPool descriptorPool;
	VkDescriptorPool samplerDescriptorPool;
	VkDescriptorPool inputDescriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;				// One for each swapchain image
	std::vector<VkDescriptorSet> samplerDescriptorSets;			// One for each texture
	std::vector<VkDescriptorSet> inputDescriptorSets;

	std::vector<VkBuffer> vpUniformBuffer;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;

	//std::vector<VkBuffer> modelDynamicUniformBuffer;
	//std::vector<VkDeviceMemory> modelDynamicUniformBufferMemory;

	// Assets

	std::vector<MeshModel> models;

	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory;
	std::vector<VkImageView> textureImageView;

	VkDeviceSize minUniformBufferOffset;
	// size_t modelUniformAlignment;
	// Model* modelTransferSpace;

	// Pipeline
	
	VkPipeline	graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkPipeline	secondPipeline;
	VkPipelineLayout secondPipelineLayout;


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
	void createPushConstantRange();
	void createGraphicsPipeline();
	void createColorBufferImage();
	void createDepthBufferImage();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronization();
	void createTextureSampler();

	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
	void createInputDescriptorSets();

	void updateUniformBuffers(uint32_t imageIndex);

	// record functinos

	void recordCommands(size_t imageIndex);

	// -------- Support Function 

	// getter functions

	void getPhysicalDevice();
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice physDevice);
	SwapchainDetails getSwapchainDetails(VkPhysicalDevice physDevice);

	// allocate functions

	void allocateDynamicBufferTransferSpace();

	// checker functions

	bool checkInstanceExtensionSupport(std::vector<const char*>* extensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	// choose functions

	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
	VkPresentModeKHR chooseBestPresentMode(const std::vector<VkPresentModeKHR>& capabilities);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkFormat chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags formatFeatureFlags);

	// create

	VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags,
		VkMemoryPropertyFlags propertyFlags, VkDeviceMemory* imageMemory);

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& fileBuffer);

	int createTextureImage(const std::string& filename);
	int createTexture(const std::string& filename);
	int createTextureDescriptor(VkImageView imageView);

	void createMeshModel(const std::string& fileName);

	// loaders

	stbi_uc* loadTextureFile(const std::string& filename, int* width, int* height, VkDeviceSize* imageSize);
};
