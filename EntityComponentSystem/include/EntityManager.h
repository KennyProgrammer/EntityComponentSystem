/*
	Author : Tobias Stein
	Date   : 11th July, 2016
	File   : EntityManager.h
	
	Enity manager class.

	All Rights Reserved. (c) Copyright 2016.
*/

#ifndef __ENTITY_MANAGER_H__
#define __ENTITY_MANAGER_H__

#include "API.h"
#include "IEntity.h"
#include "Memory/Allocator/PoolAllocator.h"

namespace ECS
{
	template<class T>
	using EntityList = std::list<T*>;

	class ECS_API EntityManager
	{
		DECLARE_LOGGER


		class IEntityContainer : protected Memory::GlobalMemoryUser
		{
		public:

			virtual ~IEntityContainer()
			{}

			virtual const char* GetEntityContainerTypeName() const = 0;

		}; // class IEntityContainer

		///-------------------------------------------------------------------------------------------------
		/// Class:	EntityContainer
		///
		/// Summary:	An entity container that manages memory chunks of enities T.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	23/09/2017
		///
		/// Typeparams:
		/// T - 	Generic type parameter.
		///-------------------------------------------------------------------------------------------------

		template<class T>
		class EntityContainer : public IEntityContainer
		{

			static const size_t ALLOC_SIZE = sizeof(T) * ENITY_T_ALLOCATION_AMOUNT;

			using TEntityList = EntityList<T>;
	
			using EnityAllocator = Memory::Allocator::PoolAllocator;
			

			struct EntityMemoryChunk
			{
				EnityAllocator*	allocator;
				TEntityList		entities;

				uptr			chunkStart;
				uptr			chunkEnd;

				EntityMemoryChunk(EnityAllocator* allocaor) :
					allocator(allocaor)
				{
					this->chunkStart = reinterpret_cast<uptr>(allocator->GetFirstMemoryAddress());
					this->chunkEnd = this->chunkStart + ALLOC_SIZE;
				}

			}; // strcut EntityMemoryChunk

			using EntityMemoryChunks = std::list<EntityMemoryChunk*>;

			EntityMemoryChunks m_Chunks;

		public:

			EntityContainer()
			{
				EnityAllocator* allocator = new EnityAllocator(ALLOC_SIZE, Allocate(ALLOC_SIZE, "EntityManager"), sizeof(T), alignof(T));
				this->m_Chunks.push_back(new EntityMemoryChunk(allocator));
			}

			virtual ~EntityContainer()
			{
				// make sure all entities will be released!
				for (auto chunk : this->m_Chunks)
				{
					for (auto ent : chunk->entities)
						((T*)ent)->~T();

					chunk->entities.clear();

					// free allocated allocator memory
					Free((void*)chunk->allocator->GetFirstMemoryAddress());
					delete chunk->allocator;
					chunk->allocator = nullptr;

					delete chunk;
					chunk = nullptr;
				}
			}

			virtual const char* GetEntityContainerTypeName() const override
			{ 
				return typeid(T).name();
			}

			///-------------------------------------------------------------------------------------------------
			/// Fn:	inline void* EntityContainer::CreateEntity()
			///
			/// Summary:	Allocates memory for a new entity of type T and returns its memory address.
			///
			/// Author:	Tobias Stein
			///
			/// Date:	23/09/2017
			///
			/// Returns:	Null if it fails, else the new entity.
			///-------------------------------------------------------------------------------------------------

			inline void* CreateEntity()
			{
				void* slot = nullptr;

				// get next free slot
				for (auto chunk : this->m_Chunks)
				{
					if (chunk->entities.size() > ENITY_T_ALLOCATION_AMOUNT)
						continue;

					slot = chunk->allocator->allocate(sizeof(T), alignof(T));
					if (slot != nullptr)
					{
						chunk->entities.push_back((T*)slot);
						break;
					}
				}

				// all chunks are full... allocate a new one
				if (slot == nullptr)
				{
					EnityAllocator* allocator = new EnityAllocator(ALLOC_SIZE, Allocate(ALLOC_SIZE, "EntityManager"), sizeof(T), alignof(T));
					EntityMemoryChunk* newChunk = new EntityMemoryChunk(allocator);

					// put new chunk in front
					this->m_Chunks.push_front(newChunk);

					slot = newChunk->allocator->allocate(sizeof(T), alignof(T));
					
					assert(slot != nullptr && "Unable to create new entity. Out of memory?!");
					newChunk->entities.push_back((T*)slot);
				}

				return slot;
			}

			inline void DestroyEntity(void* entity)
			{
				uptr adr = reinterpret_cast<uptr>(entity);

				for (auto chunk : this->m_Chunks)
				{
					if (chunk->chunkStart <= adr && chunk->chunkEnd > adr)
					{
						// note: no need to call d'tor since it was called already by 'delete'

						chunk->entities.remove((T*)entity);
						chunk->allocator->free(entity);
						return;
					}
				}

				assert(false, "Failed to delete entity. Memory corruption?!");
			}

		}; // EntityContainer

		using EntityRegistry = std::unordered_map<EntityTypeId, IEntityContainer*>;


		EntityRegistry m_EntityRegistry;

	private:	

		friend class IEntity;

		EntityManager(const EntityManager&) = delete;
		EntityManager& operator=(EntityManager&) = delete;

		using EntityLookupTable = std::vector<void*>;


		/// Summary:	Maps an entity id to object.
		EntityLookupTable m_EntityLUT;

		///-------------------------------------------------------------------------------------------------
		/// Fn:	template<class T> inline EntityContainer<T>* EntityManager::GetEntityContainer()
		///
		/// Summary:	Returns/Creates an entity container for entities of type T.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	23/09/2017
		///
		/// Typeparams:
		/// T - 	Generic type parameter.
		///
		/// Returns:	Null if it fails, else the entity container.
		///-------------------------------------------------------------------------------------------------

		template<class T>
		inline EntityContainer<T>* GetEntityContainer()
		{
			EntityTypeId EID = T::STATIC_ENTITY_TYPE_ID;

			auto it = this->m_EntityRegistry.find(EID);
			EntityContainer<T>* ec = nullptr;
						
			if (it == this->m_EntityRegistry.end())
			{
				ec = new EntityContainer<T>();
				this->m_EntityRegistry[EID] = ec;
			}
			else
				ec = (EntityContainer<T>*)it->second;

			assert(ec != nullptr && "Failed to create EntityContainer<T>!");
			return ec;
		}

		///-------------------------------------------------------------------------------------------------
		/// Fn:	EntityId EntityManager::AqcuireEntityId();
		///
		/// Summary:	Returns an unused entity id.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	23/09/2017
		///
		/// Returns:	An EntityId.
		///-------------------------------------------------------------------------------------------------

		EntityId AqcuireEntityId();

		///-------------------------------------------------------------------------------------------------
		/// Fn:	void EntityManager::ReleaseEntityId(EntityId id);
		///
		/// Summary:	Releases the entity identifier for reuse.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	23/09/2017
		///
		/// Parameters:
		/// id - 	The identifier.
		///-------------------------------------------------------------------------------------------------

		void ReleaseEntityId(EntityId id);

	public:

		EntityManager();
		~EntityManager();

		///-------------------------------------------------------------------------------------------------
		/// Fn:	template<class T> inline void* EntiyManager::CreateEntity()
		///
		/// Summary:	Creates a new entity. 
		/// DO NOT USE THAT METHOD DIRECTLY. ALWAYS USE Entity<T> class's new operator.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	11/09/2017
		///
		/// Typeparams:
		/// T - 	Generic type parameter.
		///
		/// Returns:	Null if it fails, else the new entity.
		///-------------------------------------------------------------------------------------------------

		template<class T, class... ARGS>
		inline T* CreateEntity(ARGS&&... args)
		{		
			// aqcuire memory for new entity object of type T
			void* pObjectMemory = GetEntityContainer<T>()->CreateEntity();

			// create entity inplace
			IEntity* entity = new (pObjectMemory)T(std::forward<ARGS>(args)...);

			// aqcuire unused entity id
			EntityId id = this->AqcuireEntityId();

			// set id
			entity->m_Id = id;

			return static_cast<T*>(entity);
		}

		///-------------------------------------------------------------------------------------------------
		/// Fn:	template<class T> inline void EntiyManager::DestroyEntity(void* entity)
		///
		/// Summary:	Destroies an entity.
		/// DO NOT USE THAT METHOD DIRECTLY. ALWAYS USE Entity<T> class's delete operator.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	11/09/2017
		///
		/// Typeparams:
		/// T - 	Generic type parameter.
		/// Parameters:
		/// entity - 	[in,out] If non-null, the entity.
		///-------------------------------------------------------------------------------------------------

		template<class T>
		inline void DestroyEntity(T* entity)
		{
			// free entity id
			this->ReleaseEntityId(entity->m_Id);

			// release object memory
			GetEntityContainer<T>()->DestroyEntity(entity);
		}

		///-------------------------------------------------------------------------------------------------
		/// Fn:	inline void* EntityManager::GetEntity(const EntityId id)
		///
		/// Summary:	Get an entity object by its id.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	23/09/2017
		///
		/// Parameters:
		/// id - 	The identifier.
		///
		/// Returns:	Null if it fails, else the entity.
		///-------------------------------------------------------------------------------------------------

		inline void* GetEntity(const EntityId id) 
		{
			assert((id != INVALID_ENTITY_ID && id < this->m_EntityLUT.size()) && "Invalid entity id");
			return this->m_EntityLUT[id];
		}
	};
	 
} // namespace ECS

#endif // __ENTITY_MANAGER_H__