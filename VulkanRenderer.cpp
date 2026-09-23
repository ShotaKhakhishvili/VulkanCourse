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
		getPhysicalDevice();
		createLogicalDevice();
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
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicaDevice);


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
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
	queueCreateInfo.queueCount = 1;
	float priority = 1.0f;
	queueCreateInfo.pQueuePriorities = &priority;

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
	createInfo.queueCreateInfoCount				= 1;
	createInfo.pQueueCreateInfos				= &queueCreateInfo;

	createInfo.enabledExtensionCount			= 0;
	createInfo.ppEnabledExtensionNames			= nullptr;


	VkPhysicalDeviceFeatures physDeviceFeatures{};
	createInfo.pEnabledFeatures					= &physDeviceFeatures;

	VkResult result = vkCreateDevice(mainDevice.physicaDevice, &createInfo, nullptr, &mainDevice.logicalDevice);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Logical device was not created successfully");
	}

	// queues are created at the same time as the device, so we want to handle queues
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
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
			mainDevice.physicaDevice = device;
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

		// Stop once everything we need has been found
		if (indices.isValid())
		{
			break;
		}

		i++;
	}

	return indices;
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

	return indices.isValid();
}