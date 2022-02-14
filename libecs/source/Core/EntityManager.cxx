#include "pivot/ecs/Core/EntityManager.hxx"

EntityManager::EntityManager()
{
    for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) mAvailableEntities.push(entity);
}

Entity EntityManager::CreateEntity()
{
    if (mLivingEntityCount >= MAX_ENTITIES) throw EcsException("Too many entities in existence.");

    Entity id = mAvailableEntities.front();
    mEntities.insert({id, Signature()});
    mAvailableEntities.pop();
    ++mLivingEntityCount;

    return id;
}

void EntityManager::DestroyEntity(Entity entity)
{
    if (entity >= MAX_ENTITIES) throw EcsException("Entity out of range.");

    mAvailableEntities.push(entity);
    mEntities.erase(entity);
    --mLivingEntityCount;
}

void EntityManager::SetSignature(Entity entity, Signature signature)
{
    if (entity >= MAX_ENTITIES) throw EcsException("Entity out of range.");
    mEntities[entity] = signature;
}

Signature EntityManager::GetSignature(Entity entity)
{
    if (entity >= MAX_ENTITIES) throw EcsException("Entity out of range.");
    return mEntities[entity];
}

std::unordered_map<Entity, Signature> EntityManager::getEntities() { return mEntities; }

uint32_t EntityManager::getLivingEntityCount() { return mLivingEntityCount; }
