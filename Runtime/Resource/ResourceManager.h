/*
Copyright(c) 2016-2017 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

//= INCLUDES ====================
#include <memory>
#include <map>
#include "ResourceCache.h"
#include "../Core/SubSystem.h"
#include "../Core/GameObject.h"
#include "Import/ModelImporter.h"
#include "Import/ImageImporter.h"
#include "Import/FontImporter.h"
//===============================

namespace Directus
{
	class DLL_API ResourceManager : public Subsystem
	{
	public:
		ResourceManager(Context* context);
		~ResourceManager() { Clear(); }

		//= Subsystem =============
		bool Initialize() override;
		//=========================

		// Unloads all resources
		void Clear() { m_resourceCache->Clear(); }

		// Loads a resource and adds it to the resource cache
		template <class T>
		std::weak_ptr<T> Load(const std::string& filePath)
		{
			// Try to make the path relative to the engine (in case it isn't)
			std::string relativeFilePath = FileSystem::GetRelativeFilePath(filePath);
			std::string name = FileSystem::GetFileNameNoExtensionFromFilePath(relativeFilePath);

			// Check if the resource is already loaded
			if (m_resourceCache->IsCached(filePath))
			{
				return GetResourceByName<T>(name);
			}

			// Create new resource
			std::shared_ptr<T> typed = std::make_shared<T>(m_context);

			// Assign filepath and name
			typed->SetResourceFilePath(relativeFilePath);
			typed->SetResourceName(name);

			// Load
			typed->SetLoadState(Loading);
			if (!typed->LoadFromFile(relativeFilePath))
			{
				LOG_WARNING("ResourceManager: Resource \"" + relativeFilePath + "\" failed to load");
				typed->SetLoadState(Failed);
				return std::weak_ptr<T>();
			}
			typed->SetLoadState(Completed);

			return Add(typed);
		}

		// Adds a resource into the resource cache
		template <class T>
		std::weak_ptr<T> Add(std::weak_ptr<T> resource) { return Add(resource.lock()); }

		// Adds a resource into the resource cache
		template <class T>
		std::weak_ptr<T> Add(std::shared_ptr<T> resource)
		{
			if (!resource)
				return std::weak_ptr<T>();

			// If the resource is already loaded, return the existing one
			if (m_resourceCache->IsCached(resource->GetResourceFilePath()))
				return GetResourceByName<T>(FileSystem::GetFileNameNoExtensionFromFilePath(resource->GetResourceFilePath()));

			// Add the resource
			std::shared_ptr<Resource> base = ToBaseShared(resource);
			m_resourceCache->Add(base);

			// Return it
			return resource;
		}

		template <class T>
		void SaveResource(std::weak_ptr<T> resource, const std::string& filePath)
		{
			if (resource.expired())
				return;

			resource._Get()->SetResourceFilePath(filePath);
			resource._Get()->SetResourceName(FileSystem::GetFileNameNoExtensionFromFilePath(filePath));
			resource._Get()->SaveToFile(filePath);
		}

		// Returns cached resource by ID
		template <class T>
		std::weak_ptr<T> GetResourceByID(const std::size_t ID)
		{
			return ToDerivedWeak<T>(m_resourceCache->GetByID(ID));
		}

		// Returns cached resource by Path
		template <class T>
		std::weak_ptr<T> GetResourceByName(const std::string& name)
		{
			return ToDerivedWeak<T>(m_resourceCache->GetByName(name));
		}

		// Returns cached resource by Path
		template <class T>
		std::weak_ptr<T> GetResourceByPath(const std::string& path)
		{
			return ToDerivedWeak<T>(m_resourceCache->GetByPath(path));
		}

		// Returns cached resource by Type
		template <class T>
		std::vector<std::weak_ptr<T>> GetResourcesByType()
		{
			std::vector<std::weak_ptr<T>> typedVec;
			for (const auto& resource : m_resourceCache->GetAll())
			{
				std::weak_ptr<T> typed = ToDerivedWeak<T>(resource);
				bool validCasting = !typed.expired();

				if (validCasting)
				{
					typedVec.push_back(typed);
				}
			}
			return typedVec;
		}

		// Returns all resources of a given type
		unsigned int GetResourceCountByType(ResourceType type)
		{
			return m_resourceCache->GetByType(type).size();
		}

		void SaveResourcesToFiles()
		{
			m_resourceCache->SaveResourcesToFiles();
		}

		void GetResourceFilePaths(std::vector<std::string>& filePaths)
		{
			m_resourceCache->GetResourceFilePaths(filePaths);
		}

		// Memory
		unsigned int GetMemoryUsageKB(ResourceType type) { return m_resourceCache->GetMemoryUsageKB(type); }

		// Directories
		void AddStandardResourceDirectory(ResourceType type, const std::string& directory);
		std::string GetStandardResourceDirectory(ResourceType type);
		void SetProjectDirectory(const std::string& directory);
		std::string GetProjectDirectory() { return m_projectDirectory; }

		// Importers
		std::weak_ptr<ModelImporter> GetModelImporter() { return m_modelImporter; }
		std::weak_ptr<ImageImporter> GetImageImporter() { return m_imageImporter; }
		std::weak_ptr<FontImporter> GetFontImporter() { return m_fontImporter; }

	private:
		std::unique_ptr<ResourceCache> m_resourceCache;
		std::map<ResourceType, std::string> m_standardResourceDirectories;
		std::string m_projectDirectory;

		// Importers
		std::shared_ptr<ModelImporter> m_modelImporter;
		std::shared_ptr<ImageImporter> m_imageImporter;
		std::shared_ptr<FontImporter> m_fontImporter;

		// Derived -> Base (as a shared pointer)
		template <class Type>
		static std::shared_ptr<Resource> ToBaseShared(std::shared_ptr<Type> derived)
		{
			std::shared_ptr<Resource> base = dynamic_pointer_cast<Resource>(derived);

			return base;
		}

		// Base -> Derived (as a weak pointer)
		template <class Type>
		static std::weak_ptr<Type> ToDerivedWeak(std::shared_ptr<Resource> base)
		{
			std::shared_ptr<Type> derivedShared = dynamic_pointer_cast<Type>(base);
			std::weak_ptr<Type> derivedWeak = std::weak_ptr<Type>(derivedShared);

			return derivedWeak;
		}
	};

	// Dummy template decleration to prevent 
	// errors when compiling the editor.
	template<int>
	void dynamic_pointer_cast();
}