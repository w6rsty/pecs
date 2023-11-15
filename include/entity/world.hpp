#pragma once
#include "../pecs.hpp"
#include "container/sparse_set.hpp"
#include "entity/world.hpp"
#include <_types/_uint32_t.h>
#include <errno.h>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>
#include <cassert>
#include <unordered_map>

#define assertm(msg, expr) assert(((void)msg, (expr)))

namespace pecs {

using ComponentID = uint32_t;
using Entity = uint32_t;

struct Resource {};
struct Component {};

template <typename Category>
class IndexGetter final {
private:
    inline static uint32_t curIdx_ = {};
public:
    template <typename T>
    static uint32_t Get() {
        static uint32_t id = curIdx_++; 
        return id; 
    }
};

template <typename T, typename = std::enable_if<std::is_integral_v<T>>>
class IDGenrator final {
private:
    inline static T curId_ = {};
public:
    static T Gen() { return curId_++; }
};

using EntityGenerator = IDGenrator<Entity>;

class Commands;
class Resources;
class Queryer;

class World final {
public:
    friend class Commands;
    friend class Resources;
    friend class Queryer;
    using ComponentContainer = std::unordered_map<ComponentID, void*>;

    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
private:
    struct Pool {
        std::vector<void*> instances;
        std::vector<void*> cache;

        using CreateFunc = void*(*)(void);
        using DestoryFunc = void(*)(void*);

        CreateFunc create;
        DestoryFunc destory;

        Pool(CreateFunc create, DestoryFunc destory) : create(create), destory(destory) {}

        void* Create() {
            if (!cache.empty()) {
                instances.push_back(cache.back());
                cache.pop_back();
            } else {
                instances.push_back(create());
            }
            return instances.back();
        }

        void Destory(void* elem) {
            if (auto it = std::find(instances.begin(), instances.end(), elem); it != instances.end()) {
                cache.push_back(*it);
                std::swap(*it, instances.back());
                instances.pop_back();
            } else {
                assertm("Element not in pool", false);
            }
        } 
    };

    struct ComponentInfo {
        Pool pool;
        SparseSet<Entity, PECS_SPARSE_PAGE> sparseSet;

        ComponentInfo(Pool::CreateFunc create, Pool::DestoryFunc destroy) : pool(create, destroy) {}
        ComponentInfo() : pool(nullptr, nullptr) {}
    };

    // 每种Component对应一个Pool和一个SparseSet
    using ComponentMap = std::unordered_map<ComponentID, ComponentInfo>;
    // 储存所有组件
    ComponentMap componentMap_;
    std::unordered_map<Entity, ComponentContainer> entities_;

    struct ResourceInfo {
        void* resource = nullptr;
        using DestroyFunc = void(*)(void*);

        DestroyFunc destroy = nullptr;

        ResourceInfo(DestroyFunc destory) : destroy(destory) {
            assertm("Destory function can not be null", destory);
        }
        ResourceInfo() = default;
        ~ResourceInfo() {
            destroy(resource);
        }
    };

    std::unordered_map<ComponentID, ResourceInfo> resources_;
};

class Commands final {
private:
    World& world_;
public:
    Commands(World& world) : world_(world) {}

    // 创建新的Entity
    template <typename... ComponentTypes>
    Commands& spawn(ComponentTypes&&... components) {
        spawnAndReturn<ComponentTypes...>(std::forward<ComponentTypes>(components)...);
        return *this;
    }

    template <typename... ComponentTypes>
    Entity spawnAndReturn(ComponentTypes&&... components) {
        Entity entity = EntityGenerator::Gen();
        doSpawn(entity, std::forward<ComponentTypes>(components)...);
        return entity;
    }

    Commands& destory(Entity entity) {
        if (auto it = world_.entities_.find(entity); it != world_.entities_.end()) {
            for (auto [id, component] : it->second) {
                auto& componentInfo = world_.componentMap_[id];
                componentInfo.pool.Destory(component);
                componentInfo.sparseSet.remove(entity);
            }
            world_.entities_.erase(it);
        }
        return *this;
    }

    template <typename T>
    Commands& setResource(T&& resource) {
        auto index = IndexGetter<Resource>::Get<T>();
        if (auto it = world_.resources_.find(index); it != world_.resources_.end()) {
            assertm("Resources already exsits", it->second.resource);
            it->second.resource = new T(std::forward<T>(resource));
        } else {
            auto newIt = world_.resources_.emplace(index, World::ResourceInfo([](void* elem) { delete (T*)elem; }));
            newIt.first->second.resource = new T;
            *(T*)(newIt.first->second.resource) = std::forward<T>(resource);
        }
        return *this;
    }

    template <typename T>
    Commands& removeResource() {
        auto index = IndexGetter<Resource>::Get<T>();
        if (auto it = world_.resources_.find(index); it != world_.resources_.end()) {
            delete (T*)it->second.resource;
            it->second.resource = nullptr;
        }
        return *this;
    }
    
private:
    template <typename T, typename... Remains>
    void doSpawn(Entity entity, T&& component, Remains&&... remains) {
        auto index = IndexGetter<Component>::Get<T>(); // 每种Component都会具有单独的Index计数器
        if (auto it = world_.componentMap_.find(index); it == world_.componentMap_.end()) {
            // 没有找到则生成新的Index
            // TODO: might unecessary, need test
            world_.componentMap_.emplace(index, World::ComponentInfo(
                []()->void* { return new T; },
                [](void* elem) { delete (T*)(elem); }
            ));
        }
        World::ComponentInfo& componentInfo = world_.componentMap_[index];
        void* elem = componentInfo.pool.Create();
        *((T*)elem) = std::forward<T>(component);
        componentInfo.sparseSet.add(entity);

        auto it = world_.entities_.emplace(entity, World::ComponentContainer{});
        it.first->second[index] = elem;

        // 递归可变参数
        if constexpr (sizeof...(remains) != 0) {
            doSpawn<Remains...>(entity, std::forward<Remains>(remains)...);
        }
    }
};

class Resources final {
private:
    World& world_;
public:
    Resources(World& world) : world_(world) {}

    template <typename T>
    bool has() const {
        auto index = IndexGetter<Resource>::Get<T>();
        auto it = world_.resources_.find(index);
        return it != world_.resources_.end() && it->second.resource;
    }

    template <typename T>
    T& get() {
        auto index = IndexGetter<Resource>::Get<T>();
        return *((T*)world_.resources_[index].resource);
    }
};

class Queryer final {
private:
    World& world_;
public:
    Queryer(World& world) : world_(world) {}

    template <typename... Components>
    std::vector<Entity> query() {
        std::vector<Entity> entities;
        doQuery<Components...>(entities);
        return entities;
    }

    template <typename T>
    bool has(Entity entity) {
        auto it = world_.entities_.find(entity);
        auto index = IndexGetter<Component>::Get<T>();
        return it != world_.entities_.end() && it->second.find(index) != it->second.end();
    }

    template <typename T>
    T& get(Entity entity) {
        auto index = IndexGetter<Component>::Get<T>();
        return *((T*)world_.entities_[entity][index]);
    }
private:
    template <typename T, typename... Remains>
    bool doQuery(std::vector<Entity>& outEntities) {
        auto index = IndexGetter<Component>::Get<T>();
        World::ComponentInfo& info  = world_.componentMap_[index];

        for (auto e : info.sparseSet) {
            if constexpr (sizeof...(Remains) != 0) {
                doQueryRemains<Remains...>(e, outEntities);
            } else {
                outEntities.push_back(e);
            }
        }
        return !outEntities.empty();
    }

    template <typename T, typename... Remains>
    bool doQueryRemains(Entity entity, std::vector<Entity>& outEntites) {
        auto index = IndexGetter<Component>::Get<T>();
        auto& componentContainer = world_.entities_[entity];
        if (auto it = componentContainer.find(index); it == componentContainer.end()) {
            return false;
        }

        if constexpr (sizeof...(Remains) == 0) {
            outEntites.push_back(entity);
            return true;
        } else {
            doQueryRemains<Remains...>(entity, outEntites);
        }
    }
};

}