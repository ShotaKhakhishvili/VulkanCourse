#include "VulkanRenderer.h"

#include <algorithm>
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