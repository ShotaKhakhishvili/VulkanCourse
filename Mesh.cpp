#include "Mesh.h"

#include <stdexcept>
#include <cstring>

int Mesh::getVertexCount()
{
    return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
    return vertexBuffer;
}

void Mesh::destroyVertexBuffer()
{
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
}

Mesh::~Mesh()
{
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices)
{
    // Get size of the buffer needed for vertices
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

    // Temp buffer to "stage" vertex data before trarnsferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, &stagingBufferMemory);

    // Map memory to vertex buffer
    void* data;                                                             // Create pointer to point in normal memory
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);  // Map the vertex buffer memory to point to that pointer
    memcpy(data, vertices->data(), (size_t)bufferSize);                // Copy memory from vertices vertor to the pointer
    vkUnmapMemory(device, vertexBufferMemory);                              // Unmap the vertex buffer memory

    // Create buffer with TRANSFER_DST_BIT to mark it as a receiving end of the transfer data
    createBuffer(physicalDevice, device, bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, &vertexBufferMemory);

    // Copy the staging buffer to the vertex buffer on GPU
    copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

    // Destroy staging buffer parts
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}


Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
    VkCommandPool transferCommandPool, std::vector<Vertex>* vertices)
{
    vertexCount = vertices->size();
    physicalDevice = newPhysicalDevice;
    device = newDevice;
    createVertexBuffer(transferQueue, transferCommandPool, vertices);
}
