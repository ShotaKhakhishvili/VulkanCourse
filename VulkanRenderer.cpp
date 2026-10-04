#include "VulkanRenderer.h"

#include <algorithm>
#include <limits>
#include <set>
#include <cstring>
#include <cstdio>

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
		createRenderPass();
		createGraphicsPipeline();
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

void VulkanRenderer::createInstance()
{
	/*
		VkStructureType    sType;
		const void*        pNext;
		const char*        pApplicationName;
		uint32_t           applicationVersion;
		const char*        pEngineName;
		uint32_t           engineVersion;
		uint32_t           apiVersion;
	*/
	VkApplicationInfo appInfo{};

	appInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext				= nullptr;
	appInfo.pApplicationName	= "Vulkan App";
	appInfo.applicationVersion	= VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName			= "None";
	appInfo.engineVersion		= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion			= VK_API_VERSION_1_4;

	/*
		VkStructureType             sType;
		const void*                 pNext;
		VkInstanceCreateFlags       flags;
		const VkApplicationInfo*    pApplicationInfo;
		uint32_t                    enabledLayerCount;
		const char* const*          ppEnabledLayerNames;
		uint32_t                    enabledExtensionCount;
		const char* const*          ppEnabledExtensionNames;
	*/
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
		/*
			VkStructureType             sType;
			const void*                 pNext;
			VkDeviceQueueCreateFlags    flags;
			uint32_t                    queueFamilyIndex;
			uint32_t                    queueCount;
			const float*                pQueuePriorities;
		*/
		VkDeviceQueueCreateInfo queueCreateInfo{};

		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	/*
	    VkStructureType                    sType;
		const void*                        pNext;
		VkDeviceCreateFlags                flags;
		uint32_t                           queueCreateInfoCount;
		const VkDeviceQueueCreateInfo*     pQueueCreateInfos;
		// enabledLayerCount is legacy and not used
		uint32_t                           enabledLayerCount;
		// ppEnabledLayerNames is legacy and not used
		const char* const*                 ppEnabledLayerNames;
		uint32_t                           enabledExtensionCount;
		const char* const*                 ppEnabledExtensionNames;
		const VkPhysicalDeviceFeatures*    pEnabledFeatures; 
	*/
	VkDeviceCreateInfo createInfo{};
	createInfo.sType							= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount				= static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos				= queueCreateInfos.data();

	createInfo.enabledExtensionCount			= static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames			= deviceExtensions.data();


	VkPhysicalDeviceFeatures physDeviceFeatures{};
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

	/*
	    VkStructureType                  sType;
		const void*                      pNext;
		VkSwapchainCreateFlagsKHR        flags;
		VkSurfaceKHR                     surface;
		uint32_t                         minImageCount;
		VkFormat                         imageFormat;
		VkColorSpaceKHR                  imageColorSpace;
		VkExtent2D                       imageExtent;
		uint32_t                         imageArrayLayers;
		VkImageUsageFlags                imageUsage;
		VkSharingMode                    imageSharingMode;
		uint32_t                         queueFamilyIndexCount;
		const uint32_t*                  pQueueFamilyIndices;
		VkSurfaceTransformFlagBitsKHR    preTransform;
		VkCompositeAlphaFlagBitsKHR      compositeAlpha;
		VkPresentModeKHR                 presentMode;
		VkBool32                         clipped;
		VkSwapchainKHR                   oldSwapchain;
	*/

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
	std::vector<VkImage> swapchainImages;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, swapchainImages.data());

	for (VkImage image : swapchainImages)
	{
		// store the image handles
		SwapchainImage newImage{};
		newImage.image = image;
		newImage.imageView = createImageView(image, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}

}

void VulkanRenderer::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format			= swapchainImageFormat;
	colorAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;


	std::array<VkSubpassDependency, 2> subpassDependencies;
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount	= 1;
	renderPassCreateInfo.pAttachments		= &colorAttachment;
	renderPassCreateInfo.subpassCount		= 1;
	renderPassCreateInfo.pSubpasses			= &subpass;
	renderPassCreateInfo.dependencyCount	= static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies		= subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a render pass");
	}
}

void VulkanRenderer::createGraphicsPipeline()
{
	auto vertShader = readFile("Shaders/vert.spv");
	auto fragShader = readFile("Shaders/frag.spv");

	VkShaderModule vertShaderModule = createShaderModule(vertShader);
	VkShaderModule fragShaderModule = createShaderModule(fragShader);

	/*
	    VkStructureType                     sType;
		const void*                         pNext;
		VkPipelineShaderStageCreateFlags    flags;
		VkShaderStageFlagBits               stage;
		VkShaderModule                      module;
		const char*                         pName;
		const VkSpecializationInfo*         pSpecializationInfo; 
	*/

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

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};

	vertexInputCreateInfo.sType								= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount		= 0;
	vertexInputCreateInfo.pVertexBindingDescriptions		= nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount	= 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions		= nullptr;

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
	rasterizerCreateInfo.frontFace					= VK_FRONT_FACE_CLOCKWISE;
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
	colorState.blendEnable = VK_TRUE;
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

	// Pipeline Layout. TODO: Apply Future Descriptor Set Layouts
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {}; 
	pipelineLayoutCreateInfo.sType						= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount				= 0;
	pipelineLayoutCreateInfo.pSetLayouts				= nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount		= 0;
	pipelineLayoutCreateInfo.pPushConstantRanges		= nullptr;

	VkResult result= vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a pipeline layout");
	}

	// Depth stencil testing. TODO.

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType				= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount			= 2;
	pipelineCreateInfo.pStages				= shaderStages;
	pipelineCreateInfo.pVertexInputState	= &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState	= &inputAssembly;
	pipelineCreateInfo.pViewportState		= &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState		= nullptr;
	pipelineCreateInfo.pRasterizationState	= &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState	= &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState		= &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState	= nullptr; // Todo later
	pipelineCreateInfo.layout				= pipelineLayout;
	pipelineCreateInfo.renderPass			= renderPass;
	pipelineCreateInfo.subpass				= 0;

	pipelineCreateInfo.basePipelineHandle	= VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex	= -1;

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a graphics pipeline"); 
	}

	vkDestroyShaderModule(mainDevice.logicalDevice, fragShaderModule, nullptr); 
	vkDestroyShaderModule(mainDevice.logicalDevice, vertShaderModule, nullptr); 
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


bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	/* 
	
	// device info
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// what device can do
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	*/

	QueueFamilyIndices indices = getQueueFamilies(device);

	bool extensionsSupport = checkDeviceExtensionSupport(device);

	SwapchainDetails swapChainDetails = getSwapchainDetails(device);
	bool swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();

	return indices.isValid() && extensionsSupport && swapChainValid;
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
// Format:		VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_B8G8G8A8_UNORM as backup
// Colorspace:	VK_COLORSPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& surfaceFormats)
{
	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : surfaceFormats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
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

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& fileBuffer)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = fileBuffer.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t * >(fileBuffer.data());

	VkShaderModule shaderModule;

	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't create a shader module");
	}

	return shaderModule;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	/*
	    VkStructureType            sType;
		const void*                pNext;
		VkImageViewCreateFlags     flags;
		VkImage                    image;
		VkImageViewType            viewType;
		VkFormat                   format;
		VkComponentMapping         components;
		VkImageSubresourceRange    subresourceRange;
	*/
	VkImageViewCreateInfo imageViewCreateInfo{};

	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// subresource allow the view to view onnly a part of an image
	imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &imageViewCreateInfo, nullptr, &imageView);
	
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Couldn't craete an image view");
	}

	return imageView;
}
