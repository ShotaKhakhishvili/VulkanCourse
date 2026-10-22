
#include "MeshModel.h"

MeshModel::MeshModel()
{
}

MeshModel::MeshModel(std::vector<Mesh>& newMeshes)
	:meshes(newMeshes)
{
}

MeshModel::~MeshModel()
{
}

size_t MeshModel::getMeshCount() const
{
	return meshes.size();
}

Mesh* MeshModel::getMesh(size_t meshIdx)
{
	if (meshIdx >= meshes.size())
		throw std::runtime_error("Invalid mesh model index");

	return &(meshes[meshIdx]);
}

glm::mat4 MeshModel::getModel() const
{
	return model;
}

void MeshModel::setModel(glm::mat4 newModel)
{
	model = newModel;
}

std::vector<std::string> MeshModel::LoadMaterials(const aiScene* scene)
{
	// 1:1 sized list to textures
	std::vector<std::string> textures(scene->mNumMaterials);

	for (size_t i = 0; i < scene->mNumMaterials; i++)
	{
		aiMaterial* material = scene->mMaterials[i];

		textures[i] = "";

		if (material->GetTextureCount(aiTextureType_DIFFUSE))
		{
			aiString path;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
			{
				std::string fullPath = path.data;
				size_t idx = fullPath.find_last_of("/\\");
				textures[i] = fullPath.substr(idx + 1);
			}
		}
	}

	return textures;
}

std::vector<Mesh> MeshModel::LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node, const aiScene* scene, const std::vector<int>& matToText)
{
	// Go through each mesh at this node and create it to add them to the mesh list
	std::vector<Mesh> meshes;

	for (size_t i = 0; i < node->mNumMeshes; i++)
	{
		meshes.push_back(LoadMesh(newPhysicalDevice, device, transferQueue,
			transferCommandPool, scene->mMeshes[node->mMeshes[i]], scene, matToText));
	}

	for (size_t i = 0; i < node->mNumChildren; i++)
	{
		std::vector<Mesh> newMeshes = LoadNode(newPhysicalDevice, device, transferQueue, transferCommandPool, node->mChildren[i], scene, matToText);
	
		meshes.insert(meshes.end(), newMeshes.begin(), newMeshes.end());
	}

	return meshes;
}

Mesh MeshModel::LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh, const aiScene* scene, const std::vector<int>& matToText)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.resize(mesh->mNumVertices);

	aiColor3D diffuse(1.0f, 1.0f, 1.0f);
	scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);

	for (size_t i = 0; i < vertices.size(); i++)
	{
		// Set pos
		vertices[i].pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

		// Set text coords if they exist
		if (mesh->mTextureCoords[0])
		{
			vertices[i].tex = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		}
		else
		{
			vertices[i].tex = { 0,0 };
		}

		vertices[i].col = { diffuse.r, diffuse.g, diffuse.b };
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (size_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	Mesh newMesh = Mesh(newPhysicalDevice, device, transferQueue, 
		transferCommandPool, &vertices, &indices, matToText[mesh->mMaterialIndex]);

	return newMesh;
}

void MeshModel::destroyMeshModel()
{
	for (auto& mesh : meshes)
	{
		mesh.destroyBuffers();
	}
}
