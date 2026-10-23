#include "VulkanRenderer.h"

#include <algorithm>
#include <limits>
#include <set>
#include <cstring>
#include <cstdio>
#include <limits>

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* newWindow)
{
	this->window = newWindow;

	try
	{
		createInstance();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapchain();
		createDepthBufferImage();
		createColorBufferImage();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createTextureSampler();
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createInputDescriptorSets();
		createSynchronization();

		//// Create a texture image
		int testTextureImage = createTextureImage("dirt.jpg");
		int testTexture = createTexture("dirt.jpg");
		//
		//// Create a mesh
		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapchainImageExtent.width / swapchainImageExtent.height, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(20.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		uboViewProjection.projection[1][1] *= -1; // Inverting the y coord to match the screen coords

		createMeshModel("Models/MclarenP1/mclarenblend.obj");
		createMeshModel("Models/MclarenP1/accord.obj");
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::cleanup()
{
	// Wait until the device is idle, and theres nothing on the queues
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	for (size_t i = 0; i < models.size(); i++)
	{
		models[i].destroyMeshModel();
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputSetLayout, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);
	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	for (auto view : textureImageView)
		vkDestroyImageView(mainDevice.logicalDevice, view, nullptr);

	for (size_t i = 0; i < textureImages.size(); i++)
	{
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < depthBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < colorBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, colorBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, colorBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, colorBufferImageMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
		//vkDestroyBuffer(mainDevice.logicalDevice, modelDynamicUniformBuffer[i], nullptr);
		//vkFreeMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[i], nullptr);
	}

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

	for (auto image : swapchainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::updateModel(size_t meshIndex, glm::mat4 newModel)
{
	if (meshIndex >= models.size())
		throw std::runtime_error("Failed to update an object out of range");

	models[meshIndex].setModel(newModel);
}

void VulkanRenderer::draw()
{
	// Get index of the next image to be drawn to

	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	recordCommands(imageIndex);

	updateUniformBuffers(imageIndex);

	// submit command to buffer to render
	VkSubmitInfo submitInfo = {};
	VkPipelineStageFlags waitStageFlags[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	submitInfo.pWaitDstStageMask = waitStageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];

	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't submit command to graphics queue");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present image");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::createInstance()
{
	VkApplicationInfo appInfo{};

	appInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext				= nullptr;
	appInfo.pApplicationName	= "Vulkan App";
	appInfo.applicationVersion	= VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName			= "None";
	appInfo.engineVersion		= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion			= VK_API_VERSION_1_4;

	VkInstanceCreateInfo createInfo{};

	std::vector<const char*> instanceExtensions = std::vector<const char*>();

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	if (!checkInstanceExtensionSupport(&instanceExtensions))
	{
		throw std::runtime_error("VkInstance doesn't support required extensions");
	}

	createInfo.sType					= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext					= nullptr;
	createInfo.flags					= 0;
	createInfo.pApplicationInfo			= &appInfo;

	createInfo.enabledExtensionCount	= static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames	= instanceExtensions.data();

	createInfo.enabledLayerCount		= 0;
	createInfo.ppEnabledLayerNames		= nullptr;

	VkResult  result = vkCreateInstance(&createInfo, nullptr, &instance);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed To Create Vulkan Instance");
	}

}

void VulkanRenderer::createLogicalDevice()
{
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	for (auto queueFamily : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};

		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo createInfo{};
	createInfo.sType							= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount				= static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos				= queueCreateInfos.data();

	createInfo.enabledExtensionCount			= static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames			= deviceExtensions.data();


	VkPhysicalDeviceFeatures physDeviceFeatures		= {};
	physDeviceFeatures.samplerAnisotropy			= VK_TRUE;

	createInfo.pEnabledFeatures					= &physDeviceFeatures;

	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &createInfo, nullptr, &mainDevice.logicalDevice);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Logical device was not created successfully");
	}

	// queues are created at the same time as the device, so we want to handle queues
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface()
{
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a surface");
	}
}

void VulkanRenderer::createSwapchain()
{
	// Get details to choose the best settings
	SwapchainDetails swapChainDetails = getSwapchainDetails(mainDevice.physicalDevice);

	// choose best surface, presentation mode and image extent
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	VkPresentModeKHR presentationMode = chooseBestPresentMode(swapChainDetails.presentationModes);
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in swap chain?
	// Get one more than minimum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
	
	// clamp the image count to max
	// edge case is when max is 0, in which case, there is no limit
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 &&
		swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType							= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface							= surface;
	swapchainCreateInfo.imageFormat						= surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace					= surfaceFormat.colorSpace;
	swapchainCreateInfo.presentMode						= presentationMode;
	swapchainCreateInfo.imageExtent						= extent;
	swapchainCreateInfo.minImageCount					= imageCount;
	swapchainCreateInfo.imageArrayLayers				= 1;
	swapchainCreateInfo.imageUsage						= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform					= swapChainDetails.surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha					= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.clipped							= VK_TRUE;

	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// if graphics and presentation families are different, 
	// then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {
			(uint32_t)indices.graphicsFamily,
			(uint32_t)indices.presentationFamily
		};

		swapchainCreateInfo.imageSharingMode			= VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount		= 2;
		swapchainCreateInfo.pQueueFamilyIndices			= queueFamilyIndices;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount		= 0;
		swapchainCreateInfo.pQueueFamilyIndices			= nullptr;
	}

	// if old swapchain is being destroyed and this one replaces it,
	// you can link the old one to the new one to hand over the responsibilities
	swapchainCreateInfo.oldSwapchain					= VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a swapchain");
	}

	// store to reference them later
	swapchainImageFormat = surfaceFormat.format;
	swapchainImageExtent = extent;

	uint32_t swapchainImageCount = 0;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, nullptr);
	std::vector<VkImage> images(swapchainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, images.data());

	for (VkImage image : images)
	{
		// store the image handles
		SwapchainImage newImage = {};
		newImage.image			= image;
		newImage.imageView		= createImageView(image, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		swapchainImages.push_back(newImage);
	}

}

void VulkanRenderer::createRenderPass()
{
	std::array<VkSubpassDescription, 2> subpasses = {};

	// SUBPASS 1 ATTACHMENTS + RREFERENCES(INPUTS)

	// ATTACHMENTS 
	// 
	// Color attachment (INPUT)
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format					= colorFormat;
	colorAttachment.samples					= VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp					= VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp					= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.stencilLoadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout				= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment (INPUT)
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// REFERENCES

	// Color attachment (INPUT) reference
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 1;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment reference
	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 2;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Setup subpass 1
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colorAttachmentReference;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;

	// SUBPASS 2 ATTACHMENTS + REFERENCES

	// Swapchain color attachment
	VkAttachmentDescription swapchainColorAttachment	= {};
	swapchainColorAttachment.format						= swapchainImageFormat;
	swapchainColorAttachment.samples					= VK_SAMPLE_COUNT_1_BIT;
	swapchainColorAttachment.loadOp						= VK_ATTACHMENT_LOAD_OP_CLEAR;
	swapchainColorAttachment.storeOp					= VK_ATTACHMENT_STORE_OP_STORE;
	swapchainColorAttachment.stencilLoadOp				= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	swapchainColorAttachment.stencilStoreOp				= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	swapchainColorAttachment.initialLayout				= VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainColorAttachment.finalLayout				= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// REFERENCES
	
	// Color attachment reference
	VkAttachmentReference swapchainColorAttachmentReference		= {};
	swapchainColorAttachmentReference.attachment				= 0;
	swapchainColorAttachmentReference.layout					= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// References to attachments that the subpass 2 will take the inputs from
	std::array<VkAttachmentReference, 2> inputReferences;
	inputReferences[0].attachment = 1;
	inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[1].attachment = 2;
	inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Setupt subpass 2
	subpasses[1].pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].colorAttachmentCount		= 1;
	subpasses[1].pColorAttachments			= &swapchainColorAttachmentReference;
	subpasses[1].inputAttachmentCount		= static_cast<uint32_t>(inputReferences.size());
	subpasses[1].pInputAttachments			= inputReferences.data();

	// Subpass Dependencies
	std::array<VkSubpassDependency, 3> subpassDependencies;
	subpassDependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].srcStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[0].srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[0].dstSubpass		= 0;
	subpassDependencies[0].dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags	= 0;

	// Subpass 1 layout (color, depth) to subpass 2 layout
	subpassDependencies[1].srcSubpass		= 0;
	subpassDependencies[1].srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[1].dstSubpass		= 1;
	subpassDependencies[1].dstStageMask		= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[1].dstAccessMask	= VK_ACCESS_SHADER_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	subpassDependencies[2].srcSubpass		= 1;
	subpassDependencies[2].srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[2].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[2].dstSubpass		= VK_SUBPASS_EXTERNAL;
	subpassDependencies[2].dstStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[2].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	subpassDependencies[2].dependencyFlags	= 0;

	std::array<VkAttachmentDescription, 3> renderPassAttachments = {
		swapchainColorAttachment, colorAttachment, depthAttachment
	};

	VkRenderPassCreateInfo renderPassCreateInfo		= {};
	renderPassCreateInfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount			= static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments				= renderPassAttachments.data();
	renderPassCreateInfo.subpassCount				= static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses					= subpasses.data();
	renderPassCreateInfo.dependencyCount			= static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies				= subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a render pass");
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	// UNIFORM VALUES DESCRIPTOR SET LAYOUT
	
	// View and projection binding info
	VkDescriptorSetLayoutBinding vpLayoutBinding	= {};
	vpLayoutBinding.binding							= 0;
	vpLayoutBinding.descriptorType					= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpLayoutBinding.descriptorCount					= 1;
	vpLayoutBinding.stageFlags						= VK_SHADER_STAGE_VERTEX_BIT;
	vpLayoutBinding.pImmutableSamplers				= nullptr;								// For textures, makes the sampler data unchangable

	// Model binding
	//VkDescriptorSetLayoutBinding modelLayoutBinding		= {};
	//modelLayoutBinding.binding							= 1;
	//modelLayoutBinding.descriptorType					= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	//modelLayoutBinding.descriptorCount					= 1;
	//modelLayoutBinding.stageFlags						= VK_SHADER_STAGE_VERTEX_BIT;
	//modelLayoutBinding.pImmutableSamplers				= nullptr;								// For textures, makes the sampler data unchangable

	VkDescriptorSetLayoutBinding bindings[] = {
		vpLayoutBinding, /* modelLayoutBinding */
	};

	VkDescriptorSetLayoutCreateInfo createInfo	= {};
	createInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount						= 1;
	createInfo.pBindings						= &bindings[0];

	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &createInfo, nullptr, &descriptorSetLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor set layout");
	}

	// TEXTURE SAMPLER DESCRIPTOR SET LAYOUT

	// Create texture layout binding
	VkDescriptorSetLayoutBinding textureLayoutBinding	= {};
	textureLayoutBinding.binding						= 0;
	textureLayoutBinding.descriptorType					= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureLayoutBinding.descriptorCount				= 1;
	textureLayoutBinding.stageFlags						= VK_SHADER_STAGE_FRAGMENT_BIT;
	textureLayoutBinding.pImmutableSamplers				= nullptr;

	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo		= {};
	textureLayoutCreateInfo.sType								= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount						= 1;
	textureLayoutCreateInfo.pBindings							= &textureLayoutBinding;

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler descriptor set layout");
	}

	// CREATE INPUT ATTACHMENT IMAGE DESCRIPTOR SET LAYOUT

	// Color input
	VkDescriptorSetLayoutBinding colorInputLayoutBinding	= {};
	colorInputLayoutBinding.binding							= 0;
	colorInputLayoutBinding.descriptorType					= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputLayoutBinding.descriptorCount					= 1;
	colorInputLayoutBinding.stageFlags						= VK_SHADER_STAGE_FRAGMENT_BIT;
	colorInputLayoutBinding.pImmutableSamplers				= nullptr;

	// Depth input
	VkDescriptorSetLayoutBinding depthInputLayoutBinding	= {};
	depthInputLayoutBinding.binding							= 1;
	depthInputLayoutBinding.descriptorType					= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputLayoutBinding.descriptorCount					= 1;
	depthInputLayoutBinding.stageFlags						= VK_SHADER_STAGE_FRAGMENT_BIT;
	depthInputLayoutBinding.pImmutableSamplers				= nullptr;

	std::array< VkDescriptorSetLayoutBinding, 2> inputLayoutBindings = {
		colorInputLayoutBinding, depthInputLayoutBinding
	};

	// Descriptor

	VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo	= {};
	inputLayoutCreateInfo.sType								= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	inputLayoutCreateInfo.bindingCount						= static_cast<uint32_t>(inputLayoutBindings.size());
	inputLayoutCreateInfo.pBindings							= inputLayoutBindings.data();

	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler descriptor set layout");
	}
}

void VulkanRenderer::createPushConstantRange()
{
	pushConstantRange.stageFlags	= VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset		= 0;
	pushConstantRange.size			= sizeof(Model);
}

void VulkanRenderer::createGraphicsPipeline()
{
	auto vertShader = readFile("Shaders/vert.spv");
	auto fragShader = readFile("Shaders/frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertShader);
	VkShaderModule fragShaderModule = createShaderModule(fragShader);

	VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
	vertStageCreateInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStageCreateInfo.stage	= VK_SHADER_STAGE_VERTEX_BIT;
	vertStageCreateInfo.module	= vertShaderModule;
	vertStageCreateInfo.pName	= "main";

	VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
	fragStageCreateInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStageCreateInfo.stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStageCreateInfo.module	= fragShaderModule;
	fragStageCreateInfo.pName	= "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageCreateInfo, fragStageCreateInfo };

	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding			= 0;			// Can bind multiple streams of data, this defins which to define
	bindingDescription.stride			= sizeof(Vertex);
	bindingDescription.inputRate		= VK_VERTEX_INPUT_RATE_VERTEX; 
	
	// VK_VERTEX_INPUT_RATE_INDEX : Move on to the next vertex when drawing
	// VK_VERTEX_INPUT_RATE_INSTANCE : Move to a vertex of the next instance

	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;
	// Position Attribute
	attributeDescriptions[0].binding		= 0;							// Which binding the data is at (should be same as above)
	attributeDescriptions[0].location		= 0;							// Location in shader where the data will be read from
	attributeDescriptions[0].format			= VK_FORMAT_R32G32B32_SFLOAT;	// Format the data will dake (also helps define the size of the data)
	attributeDescriptions[0].offset			= offsetof(Vertex, pos);		// Offset of the pos attribute in the Vertex structure

	// Color Attribute
	attributeDescriptions[1].binding		= 0;							
	attributeDescriptions[1].location		= 1;							
	attributeDescriptions[1].format			= VK_FORMAT_R32G32B32_SFLOAT;	
	attributeDescriptions[1].offset			= offsetof(Vertex, col);

	// Texture Attribute
	attributeDescriptions[2].binding		= 0;
	attributeDescriptions[2].location		= 2;
	attributeDescriptions[2].format			= VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset			= offsetof(Vertex, tex);

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};

	vertexInputCreateInfo.sType								= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount		= 1;
	vertexInputCreateInfo.pVertexBindingDescriptions		= &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount	= static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions		= attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

	inputAssembly.sType						= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology					= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable	= VK_FALSE;

	VkViewport viewport = {};
	viewport.x			= 0.0f;
	viewport.y			= 0.0f;
	viewport.width		= (float)swapchainImageExtent.width;
	viewport.height		= (float)swapchainImageExtent.height;
	viewport.minDepth	= 0.0f;
	viewport.maxDepth	= 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainImageExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount	= 1;
	viewportStateCreateInfo.pViewports		= &viewport;
	viewportStateCreateInfo.scissorCount	= 1;
	viewportStateCreateInfo.pScissors		= &scissor;


	// -- DYNAMIC STATES --
	// Dynamic states to enable

	/*

	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);			// Dynamic Viewport	: you can resize in command buffer with vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);			// DYnamic Scissor	: you can resize in command buffer with vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	*/

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable			= VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable	= VK_FALSE;
	rasterizerCreateInfo.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth					= 1.0f;
	rasterizerCreateInfo.cullMode					= VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace					= VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable			= VK_FALSE;

	// Multisampling
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable		= VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	// Blending
	VkPipelineColorBlendAttachmentState colorState = {};
	colorState.colorWriteMask			= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	colorState.blendEnable				= VK_TRUE;
	// blending equation uses (srcColorBlendFactor * newColor) ColorBlendOp (dstColorBlendFactor * oldColor);
	colorState.srcColorBlendFactor		= VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor		= VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp				= VK_BLEND_OP_ADD;
	colorState.srcAlphaBlendFactor		= VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor		= VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp				= VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
	colorBlendingCreateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable		= VK_FALSE;
	colorBlendingCreateInfo.attachmentCount		= 1;
	colorBlendingCreateInfo.pAttachments		= &colorState;

	// Pipeline Layout

	std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts{ descriptorSetLayout, samplerSetLayout };



	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {}; 
	pipelineLayoutCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount				= static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts				= descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount		= 1;
	pipelineLayoutCreateInfo.pPushConstantRanges		= &pushConstantRange;

	VkResult result= vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a pipeline layout");
	}

	// Depth stencil
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo	= {};
	depthStencilCreateInfo.sType									= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable							= VK_TRUE;
	depthStencilCreateInfo.depthWriteEnable							= VK_TRUE;
	depthStencilCreateInfo.depthCompareOp							= VK_COMPARE_OP_LESS;
	depthStencilCreateInfo.depthBoundsTestEnable					= VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable						= VK_FALSE;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType						= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount					= 2;
	pipelineCreateInfo.pStages						= shaderStages;
	pipelineCreateInfo.pVertexInputState			= &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState			= &inputAssembly;
	pipelineCreateInfo.pViewportState				= &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState				= nullptr;
	pipelineCreateInfo.pRasterizationState			= &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState			= &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState				= &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState			= &depthStencilCreateInfo;
	pipelineCreateInfo.layout						= pipelineLayout;
	pipelineCreateInfo.renderPass					= renderPass;
	pipelineCreateInfo.subpass						= 0;

	pipelineCreateInfo.basePipelineHandle	= VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex	= -1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a graphics pipeline"); 
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, fragShaderModule, nullptr); 
	vkDestroyShaderModule(mainDevice.logicalDevice, vertShaderModule, nullptr); 

	// CREATE SECOND PIPELINE
	auto secondVertexShaderCode = readFile("Shaders/shader2_vert.spv");
	auto secondFragmentShaderCode = readFile("Shaders/shader2_frag.spv");

	VkShaderModule secondVertShaderModule = createShaderModule(secondVertexShaderCode);
	VkShaderModule secondFragShaderModule = createShaderModule(secondFragmentShaderCode);

	// Set new shaders

	vertStageCreateInfo.module = secondVertShaderModule;
	fragStageCreateInfo.module = secondFragShaderModule;

	VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertStageCreateInfo , fragStageCreateInfo };

	// No vertex data for second pass
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	depthStencilCreateInfo.depthWriteEnable = VK_FALSE;

	VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo	= {};
	secondPipelineLayoutCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	secondPipelineLayoutCreateInfo.setLayoutCount				= 1;
	secondPipelineLayoutCreateInfo.pSetLayouts					= &inputSetLayout;
	secondPipelineLayoutCreateInfo.pushConstantRangeCount		= 0;
	secondPipelineLayoutCreateInfo.pPushConstantRanges			= nullptr;

	result = vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create the second pipeline layout");
	}

	pipelineCreateInfo.pStages		= secondShaderStages;
	pipelineCreateInfo.layout		= secondPipelineLayout;
	pipelineCreateInfo.subpass		= 1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create the second graphics pipeline");
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, secondFragShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, secondVertShaderModule, nullptr);
}

void VulkanRenderer::createColorBufferImage()
{
	colorBufferImageView.resize(swapchainImages.size());
	colorBufferImageMemory.resize(swapchainImages.size());
	colorBufferImage.resize(swapchainImages.size());

	colorFormat = chooseSupportedFormat(
		{VK_FORMAT_R8G8B8A8_UNORM},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
	);

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		colorBufferImage[i] = createImage(
			swapchainImageExtent.width,
			swapchainImageExtent.height,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&colorBufferImageMemory[i]
		);

		colorBufferImageView[i] = createImageView(colorBufferImage[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

	}
}

void VulkanRenderer::createDepthBufferImage()
{
	depthBufferImageView.resize(swapchainImages.size());
	depthBufferImageMemory.resize(swapchainImages.size());
	depthBufferImage.resize(swapchainImages.size());

	depthFormat = chooseSupportedFormat(
		{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);


	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		depthBufferImage[i] = createImage(
			swapchainImageExtent.width,
			swapchainImageExtent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&depthBufferImageMemory[i]
		);	
		
		depthBufferImageView[i] = createImageView(depthBufferImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	}

}

void VulkanRenderer::createFramebuffers()
{
	swapchainFramebuffers.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 3> attachments = {
			swapchainImages[i].imageView,
			colorBufferImageView[i],
			depthBufferImageView[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};

		framebufferCreateInfo.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass		= renderPass;
		framebufferCreateInfo.attachmentCount	= attachments.size();
		framebufferCreateInfo.pAttachments		= attachments.data();
		framebufferCreateInfo.width				= swapchainImageExtent.width;
		framebufferCreateInfo.height			= swapchainImageExtent.height;
		framebufferCreateInfo.layers			= 1;

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapchainFramebuffers[i]);
	
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Couldn't create a framebuffer");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	// Get indices of queue families from device
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags				= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex	= queueFamilyIndices.graphicsFamily;

	// Creating a graphics queue family command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a graphics queue family command pool");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	commandBuffers.resize(swapchainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType			= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool		= graphicsCommandPool;
	cbAllocInfo.level			= VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers!");
	}
}

void VulkanRenderer::createSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i])
			!= VK_SUCCESS)
		{
			throw std::runtime_error("Couldn't create semaphores and/or fences");
		}
	}
}

void VulkanRenderer::createTextureSampler()
{
	VkSamplerCreateInfo createInfo = {};
	createInfo.sType						= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter					= VK_FILTER_LINEAR;
	createInfo.minFilter					= VK_FILTER_LINEAR;
	createInfo.addressModeU					= VK_SAMPLER_ADDRESS_MODE_REPEAT;	
	createInfo.addressModeV					= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW					= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.borderColor					= VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates		= VK_FALSE;
	createInfo.mipmapMode					= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias					= 0.0f;
	createInfo.minLod						= 0.0f;
	createInfo.maxLod						= 0.0f;
	createInfo.anisotropyEnable				= VK_TRUE;
	createInfo.maxAnisotropy				= 16;

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &createInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a sampler");
	}
}

void VulkanRenderer::createUniformBuffers()
{
	VkDeviceSize vpBufferSize = sizeof(uboViewProjection);
	//VkDeviceSize modelBufferSIze = modelUniformAlignment * MAX_OBJECT_COUNT;

	vpUniformBuffer.resize(swapchainImages.size());
	vpUniformBufferMemory.resize(swapchainImages.size());

	//modelDynamicUniformBuffer.resize(swapchainImages.size());
	//modelDynamicUniformBufferMemory.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMemory[i]);

		//	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSIze, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDynamicUniformBuffer[i], &modelDynamicUniformBufferMemory[i]);
		// 
	}
}

void VulkanRenderer::createDescriptorPool()
{
	// CREATE AN UNIFORM DSECRIPTOR POOL

	// View Projection pool
	VkDescriptorPoolSize vpPoolSize		= {};
	vpPoolSize.type						= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount			= static_cast<uint32_t>(vpUniformBuffer.size());

	// Model pool size
	//VkDescriptorPoolSize modelPoolSize		= {};
	//modelPoolSize.type						= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	//modelPoolSize.descriptorCount				= static_cast<uint32_t>(modelDynamicUniformBuffer.size());

	std::vector<VkDescriptorPoolSize> poolSizes = {
		vpPoolSize
	};

	VkDescriptorPoolCreateInfo createInfo	= {};
	createInfo.sType						= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets						= static_cast<uint32_t>(swapchainImages.size());
	createInfo.poolSizeCount				= static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes					= poolSizes.data();

	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &createInfo, nullptr, &descriptorPool);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a ddescriptor pool");
	}

	// CREATE SAMPLER DESCRIPTOR POOL

	// Texture sampler
	VkDescriptorPoolSize samplerPoolSize	= {};
	samplerPoolSize.type					= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount			= MAX_OBJECT_COUNT;

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo	= {};
	samplerPoolCreateInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets						= MAX_OBJECT_COUNT;
	samplerPoolCreateInfo.poolSizeCount					= 1;
	samplerPoolCreateInfo.pPoolSizes					= &samplerPoolSize;

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a sampler descriptor pool");
	}

	// Create input attachment descriptor pool
	
	// Color input attachment pool size
	VkDescriptorPoolSize colorInputPoolSize = {};
	colorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputPoolSize.descriptorCount = static_cast<uint32_t>(colorBufferImageView.size());
	// Depth input attachment pool size
	VkDescriptorPoolSize depthInputPoolSize = {};
	depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

	std::array< VkDescriptorPoolSize, 2> inputPoolSizes = {
		colorInputPoolSize, depthInputPoolSize
	};

	VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
	inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	inputPoolCreateInfo.maxSets = swapchainImages.size();
	inputPoolCreateInfo.poolSizeCount = inputPoolSizes.size();
	inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a input descriptor pool");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	descriptorSets.resize(swapchainImages.size());

	std::vector<VkDescriptorSetLayout> layouts(swapchainImages.size(), descriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo	= {};
	allocInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool				= descriptorPool;
	allocInfo.descriptorSetCount			= static_cast<uint32_t>(swapchainImages.size());
	allocInfo.pSetLayouts					= layouts.data();

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &allocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets");
	}

	std::vector<VkWriteDescriptorSet> setWrites(swapchainImages.size() /* * 2 */ );
	std::vector<VkDescriptorBufferInfo> bufferInfos(swapchainImages.size() /* * 2 */);

	for (size_t i = 0, j = swapchainImages.size(); i < swapchainImages.size(); i++, j++)
	{
		// View Project buffer infos
		bufferInfos[i].buffer				= vpUniformBuffer[i];
		bufferInfos[i].offset				= 0;
		bufferInfos[i].range				= sizeof(uboViewProjection);

		setWrites[i].sType					= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		setWrites[i].dstSet					= descriptorSets[i];
		setWrites[i].dstBinding				= 0;
		setWrites[i].dstArrayElement		= 0;
		setWrites[i].descriptorType			= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setWrites[i].descriptorCount		= 1;
		setWrites[i].pBufferInfo			= &bufferInfos[i];

		// Model buffer infos
		//bufferInfos[j].buffer			= modelDynamicUniformBuffer[i];
		//bufferInfos[j].range			= sizeof(UboModel);
		//bufferInfos[j].offset			= 0;
		//
		//setWrites[j].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		//setWrites[j].dstSet				= descriptorSets[i];
		//setWrites[j].dstBinding			= 1;
		//setWrites[j].dstArrayElement	= 0;
		//setWrites[j].descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		//setWrites[j].descriptorCount	= 1;
		//setWrites[j].pBufferInfo		= &bufferInfos[j];
	}

	vkUpdateDescriptorSets(mainDevice.logicalDevice, setWrites.size(), setWrites.data(), 0, nullptr);
}

void VulkanRenderer::createInputDescriptorSets()
{
	inputDescriptorSets.resize(swapchainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), inputSetLayout);

	VkDescriptorSetAllocateInfo allocInfo	= {};
	allocInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool				= inputDescriptorPool;
	allocInfo.descriptorSetCount			= static_cast<uint32_t>(swapchainImages.size());
	allocInfo.pSetLayouts					= setLayouts.data();

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &allocInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate input attachment descriptor sets");
	}

	std::vector<VkWriteDescriptorSet> setWrites(swapchainImages.size() * 2);
	std::vector<VkDescriptorImageInfo> imageInfos(swapchainImages.size() * 2);

	// Update each descriptor set with input attachment
	for (size_t i = 0; i < setWrites.size(); i += 2)
	{
		// Color image 
		imageInfos[i].imageLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i].imageView			= colorBufferImageView[i>1];
		imageInfos[i].sampler			= nullptr;

		setWrites[i].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		setWrites[i].dstSet				= inputDescriptorSets[i>>1];
		setWrites[i].dstBinding			= 0;
		setWrites[i].dstArrayElement	= 0;
		setWrites[i].descriptorType		= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		setWrites[i].descriptorCount	= 1;
		setWrites[i].pImageInfo			= &imageInfos[i];

		// Depth image 
		imageInfos[i+1].imageLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[i+1].imageView		= depthBufferImageView[i>>1];
		imageInfos[i+1].sampler			= nullptr;

		setWrites[i+1].sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		setWrites[i+1].dstSet			= inputDescriptorSets[i>>1];
		setWrites[i+1].dstBinding		= 1;
		setWrites[i+1].dstArrayElement	= 0;
		setWrites[i+1].descriptorType	= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		setWrites[i+1].descriptorCount	= 1;
		setWrites[i+1].pImageInfo		= &imageInfos[i+1];
	}

	// Update descriptor sets
	vkUpdateDescriptorSets(mainDevice.logicalDevice, setWrites.size(), setWrites.data(), 0, nullptr);
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	// Copy View Project data
	void* data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(uboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	// Copy Model data
	//for (size_t i = 0; i < meshes.size(); i++)
	//{
	//	UboModel* object = (UboModel*)((uint64_t)(modelTransferSpace)+(i * modelUniformAlignment));
	//	*object = meshes[i].getModel();
	//}
	//vkMapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[imageIndex], 0, \
	//	modelUniformAlignment * meshes.size(), 0, &data);
	//memcpy(data, modelTransferSpace, modelUniformAlignment * meshes.size());
	//vkUnmapMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::recordCommands(size_t imageIndex)
{
	// information about how to begin each command buffer

	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType	= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	std::array<VkClearValue, 3> clearValues = {};

	clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[1].color = { 0.05f, 0.15f, 0.25f, 1.0f };
	clearValues[2].depthStencil.depth = 1;

	// Info about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType					= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass				= renderPass;
	renderPassBeginInfo.renderArea.offset		= { 0,0 };
	renderPassBeginInfo.renderArea.extent		= swapchainImageExtent;
	renderPassBeginInfo.pClearValues			= clearValues.data();
	renderPassBeginInfo.clearValueCount			= static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];

	// Start recording to the command buffer

	VkResult result = vkBeginCommandBuffer(commandBuffers[imageIndex], &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a command buffer");
	}

	vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Bind pipeline to be used in render pass
	vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	for (MeshModel& model : models)
	{
		glm::mat4 modelMat= model.getModel();
		vkCmdPushConstants(
			commandBuffers[imageIndex],
			pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(Model),
			&modelMat
		);

		for (size_t j = 0; j < model.getMeshCount(); j++)
		{
			VkBuffer vertexBuffers[] = { model.getMesh(j)->getVertexBuffer() };																			// Vertex buffers to bind and draw
			VkDeviceSize offsets[] = { 0 };																										// Offsets of the data from the bufferss they are written to

			// Command to bind vertex buffer before drawing
			vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);

			// Command to bind index buffer before drawing
			vkCmdBindIndexBuffer(commandBuffers[imageIndex], model.getMesh(j)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			// Dynamic offset amount
			// uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;

			std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[imageIndex], samplerDescriptorSets[model.getMesh(j)->getTexId()] };

			vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
				0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

			vkCmdDrawIndexed(commandBuffers[imageIndex], model.getMesh(j)->getIndexCount(), 1, 0, 0, 0);
		}
	}
	vkCmdNextSubpass(commandBuffers[imageIndex], VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
	vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout,
		0, 1, &inputDescriptorSets[imageIndex], 0, nullptr);
	vkCmdDraw(commandBuffers[imageIndex], 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffers[imageIndex]);

	result = vkEndCommandBuffer(commandBuffers[imageIndex]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording a command buffer");
	}
}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*>* extensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensionProperties(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, &(extensionProperties[0]));

	for (auto& extension : *extensions)
	{
		if (!std::any_of(extensionProperties.begin(), extensionProperties.end(),
				[&](const VkExtensionProperties& prop)
				{
					return std::strcmp(prop.extensionName, extension) == 0;
				}
			)	
		)	return false;
	}

	return true;
}

void VulkanRenderer::getPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't Find Any GPU");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto& device : devices)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice physDevice)
{
	QueueFamilyIndices indices{};

	uint32_t queueFamiliesCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamiliesCount, nullptr);

	std::vector<VkQueueFamilyProperties> properties(queueFamiliesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamiliesCount, properties.data());

	int i = 0;
	for (const auto& queueFamily : properties)
	{
		// Family must have at least one queue, and support graphics
		if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			indices.graphicsFamily = i;
		}

		VkBool32 presentationFamilyFound = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, i, surface, &presentationFamilyFound);
		if (queueFamily.queueCount > 0 && presentationFamilyFound)
		{
			indices.presentationFamily = i;
		}

		// Stop once everything we need has been found
		if (indices.isValid())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapchainDetails VulkanRenderer::getSwapchainDetails(VkPhysicalDevice physDevice)
{
	SwapchainDetails swapChainDetails;
	
	// Capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &swapChainDetails.surfaceCapabilities);

	// Formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatCount, swapChainDetails.formats.data());
	}

	// Presentation Modes

	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentationCount, nullptr);

	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}
	return swapChainDetails;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	//modelUniformAlignment = (sizeof(UboModel) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);
	//
	//modelTransferSpace = (UboModel*)_aligned_malloc(modelUniformAlignment * MAX_OBJECT_COUNT, modelUniformAlignment);
}


bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	/*
	
	// device info
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// what device can do
	*/

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = getQueueFamilies(device);

	bool extensionsSupport = checkDeviceExtensionSupport(device);

	SwapchainDetails swapChainDetails = getSwapchainDetails(device);
	bool swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();

	return indices.isValid() && extensionsSupport && swapChainValid && deviceFeatures.samplerAnisotropy;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);


	if (extensionCount == 0)
	{
		throw std::runtime_error("No support for device extensions");
	}

	std::vector<VkExtensionProperties> extensionProperties(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, &(extensionProperties[0]));

	for (auto& extension : deviceExtensions)
	{
		if (!std::any_of(extensionProperties.begin(), extensionProperties.end(),
			[&](const VkExtensionProperties& prop)
			{
				return std::strcmp(prop.extensionName, extension) == 0;
			}
		)
			)	return false;
	}

	return true;
}

// The best format is subjective, and ours will be:
// Format:		VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_B8G8R8A8_UNORM as backup
// Colorspace:	VK_COLORSPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
{
	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : surfaceFormats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
			format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	return surfaceFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
	for (const auto& presentMode : presentModes)
	{
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	// FIFO is always present, so if mailbox was not supported, then we use fifo.
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	// if current extent is at numeric max, then it can very. otherwise it is the size of the window.
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}

	// otherwise, we make set it manually
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	VkExtent2D newExtent{};
	newExtent.width = static_cast<uint32_t>(width);
	newExtent.height = static_cast<uint32_t>(height);

	// make sure to clamp the value in range of min and max
	newExtent.width = std::clamp(newExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	newExtent.height = std::clamp(newExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return newExtent;
}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags formatFeatureFlags)
{
	for (const VkFormat& format : formats)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & formatFeatureFlags) == formatFeatureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a matching format");
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& fileBuffer)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo		= {};
	shaderModuleCreateInfo.sType						= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize						= fileBuffer.size();
	shaderModuleCreateInfo.pCode						= reinterpret_cast<const uint32_t * >(fileBuffer.data());

	VkShaderModule shaderModule;

	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a shader module");
	}

	return shaderModule;
}

int VulkanRenderer::createTextureImage(const std::string& filename)
{
	// Load in the image file
	int width, height;
	VkDeviceSize imageSize;

	// Load in the texture file
	stbi_uc* imageData = loadTextureFile(filename, &width, &height, &imageSize);

	// Create staging buffer to hold the data, ready to copy to the device
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(mainDevice.logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, stagingBufferMemory);

	stbi_image_free(imageData);

	// Create image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;

	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

	// Copy data to image

	// Transition of image layout from undefined to transition dst optimal
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, stagingBuffer, texImage, width, height);

	// Transition of image layout from transition dst optimal to readonly optimal
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool,
		texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Add texture data to vector for future reference
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy staging buffers
	vkDestroyBuffer(mainDevice.logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, stagingBufferMemory, nullptr);

	return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(const std::string& filename)
{
	int textureImageLoc = createTextureImage(filename);

	// Create image view and add it to the list
	VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageView.push_back(imageView);

	// TODO: create descriptor set here
	int descriptorLoc = createTextureDescriptor(imageView);
	
	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView imageView)
{
	VkDescriptorSet descriptorSet;

	VkDescriptorSetAllocateInfo setAllocInfo	= {};
	setAllocInfo.sType							= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool					= samplerDescriptorPool;
	setAllocInfo.descriptorSetCount				= 1;
	setAllocInfo.pSetLayouts					= &samplerSetLayout;

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture descriptor sets");
	}

	VkDescriptorImageInfo imageInfo		= {};
	imageInfo.imageLayout				= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;		// Image layout when in use
	imageInfo.imageView					= imageView;
	imageInfo.sampler					= textureSampler;

	VkWriteDescriptorSet descriptorWrite	= {};
	descriptorWrite.sType					= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet					= descriptorSet;
	descriptorWrite.dstBinding				= 0;
	descriptorWrite.dstArrayElement			= 0;
	descriptorWrite.descriptorType			= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount			= 1;
	descriptorWrite.pImageInfo				= &imageInfo;

	// Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to the list
	samplerDescriptorSets.push_back(descriptorSet);

	return samplerDescriptorSets.size() - 1;
}

void VulkanRenderer::createMeshModel(const std::string& fileName)
{
	// Include model scene
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(fileName, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	if (!scene)
	{
		throw std::runtime_error("Failed to import a model {" + fileName + "}");
	}

	// Get vector of all materials with 1:1 ID placement
	std::vector<std::string> textures = MeshModel::LoadMaterials(scene);

	// Material IDs to texture IDs
	std::vector<int> matToTex(textures.size());

	// Loop over texture names and create textures for them
	for (size_t i = 0; i < textures.size(); i++)
	{
		if (textures[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			matToTex[i] = createTexture(textures[i]);
		}
	}

	std::vector<Mesh> modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice,
		graphicsQueue, graphicsCommandPool, scene->mRootNode, scene, matToTex);

	models.push_back(MeshModel(modelMeshes));
}

stbi_uc* VulkanRenderer::loadTextureFile(const std::string& filename, int* width, int* height, VkDeviceSize* imageSize)
{
	// Number of channels image uses
	int channels;

	// Load pixel data from an image
	std::string fileLoc = "Textures/" + filename;
	stbi_uc* image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image)
		throw std::runtime_error("Failed to load an texture file: {" + filename + "}");

	// Calculate image size using given and known data
	(*imageSize) = (*width) * (*height) * 4;

	return image;
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory* imageMemory)
{
	// Create image handle
	VkImageCreateInfo createInfo	= {};
	createInfo.sType				= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType			= VK_IMAGE_TYPE_2D;
	createInfo.extent.width			= width;
	createInfo.extent.height		= height;
	createInfo.extent.depth			= 1;
	createInfo.mipLevels			= 1;
	createInfo.arrayLayers			= 1;
	createInfo.format				= format;
	createInfo.tiling				= tiling;
	createInfo.initialLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.usage				= usageFlags;
	createInfo.samples				= VK_SAMPLE_COUNT_1_BIT;
	createInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;

	VkImage image;

	VkResult result = vkCreateImage(mainDevice.logicalDevice, &createInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image");
	}

	// Create memory for the image

	// Get memory requirements for the type of image

	VkMemoryRequirements memoryRequirements;

	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo	= {};
	allocInfo.sType					= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize		= memoryRequirements.size;
	allocInfo.memoryTypeIndex		= findMemoryTypeIndex(mainDevice.physicalDevice, 
										memoryRequirements.memoryTypeBits, propertyFlags);

	// Allocate image using requirements and uesr defined properties
	result = vkAllocateMemory(mainDevice.logicalDevice, &allocInfo, nullptr, imageMemory);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate data for an image");
	}

	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{

	VkImageViewCreateInfo imageViewCreateInfo{};

	imageViewCreateInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image			= image;
	imageViewCreateInfo.viewType		= VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format			= format;
	imageViewCreateInfo.components.r	= VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g	= VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b	= VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a	= VK_COMPONENT_SWIZZLE_IDENTITY;

	// subresource allow the view to view onnly a part of an image
	imageViewCreateInfo.subresourceRange.aspectMask			= aspectFlags;
	imageViewCreateInfo.subresourceRange.baseMipLevel		= 0;
	imageViewCreateInfo.subresourceRange.levelCount			= 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer		= 0;
	imageViewCreateInfo.subresourceRange.layerCount			= 1;

	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);
	
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't craete an image view");
	}

	return imageView;
}
