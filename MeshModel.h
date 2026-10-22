#pragma once

#include "glm/glm.hpp"
#include <assimp/scene.h>
#include "Mesh.h"
#include <vector>

class MeshModel
{
public:
	MeshModel();
	MeshModel(std::vector<Mesh>& newMeshes);
	~MeshModel();

	size_t getMeshCount()const;
	Mesh* getMesh(size_t meshIdx);

	glm::mat4 getModel()const;
	void setModel(glm::mat4 newModel);

	static std::vector<std::string> LoadMaterials(const aiScene* scene);
	static std::vector<Mesh> LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice device,
					VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node,
					const aiScene* scene, const std::vector<int>& matToText);

	static Mesh LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice device,
		VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh,
		const aiScene* scene, const std::vector<int>& matToText);


	void destroyMeshModel();

private:
	std::vector<Mesh> meshes;
	glm::mat4 model;
};

