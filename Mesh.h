#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utilities.h"

struct Model
{
	glm::mat4 model;
};

class Mesh
{
public:
	Mesh();
	Mesh(VkPhysicalDevice physicalDevice, VkDevice newDevice,VkQueue transferQueue,
		VkCommandPool transferCommandPool, std::vector<Vertex> * vertices, std::vector<uint32_t> * indices);

	void setModel(glm::mat4 newModel);
	Model& getModel();

	size_t getVertexCount();
	VkBuffer getVertexBuffer();

	size_t getIndexCount();
	VkBuffer getIndexBuffer();

	void destroyBuffers();

	~Mesh();

private:
	Model model;

	size_t vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	size_t indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;

	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices);
};