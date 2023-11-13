#pragma once
#include "container/sparse_set.hpp"
#include "entity/world.hpp"
#include <type_traits>
#include <vector>
#include <algorithm>
#include <cassert>
#include <unordered_map>

#define assertm(msg, expr) assert(((void)msg, (expr)))

namespace pecs {

using ComponentID = uint32_t;
using Entity = uint32_t;

class IndexGetter final {
public:
    template <typename T>
    static ComponentID Get() {
        static ComponentID id = _curIdx++;
        return id;
    }
private:
    inline static ComponentID _curIdx = 0;
};

template <typename T, typename = std::enable_if<std::is_integral_v<T>>>
struct IDGenerator {
public:
    static T Gen() {
        return _curId++;
    }
private:
    inline static T _curId = {};
};

using EntityGenerator = IDGenerator<Entity>;

class Commands;

class World final {
public:
    friend class Commands;
    using ComponentContainer = std::unordered_map<ComponentID, void*>;
private:
    struct Pool final {
        std::vector<void*> instances;
        std::vector<void*> cache;

        using CreateFunc = void*(*)(void);
        using DestoryFunc = void*(*)(void);

        CreateFunc create;
        DestoryFunc destory;

        Pool(CreateFunc create, DestoryFunc destory) : create(create), destory(destory) {}

        void* Create() {
            if (!cache.empty()) {
                instances.push_back(cache.back());
                cache.pop_back();
                return instances.back();
            } else {
                instances.push_back(create());
                return instances.back();
            }
        }

        void* Destory(void* elem) {
            if (auto it = std::find(instances.begin(), instances.end(), elem); it != instances.end()) {
                cache.push_back(*it);
                std::swap(*it, instances.back());
                instances.pop_back();
            } else {
                assertm("element not in pool", false);
            }
        }
    };

    struct ComponentInfo {
        Pool pool;
        SparseSet<Entity, 32> sparseSet;

        void addEntity(Entity entity) {
            sparseSet.add(entity);
        }

        void removeEnity(Entity entity) {
            sparseSet.remove(entity);
        }
    };
    
    using ComponentMap = std::unordered_map<ComponentID, ComponentInfo>;
    ComponentMap _componentMap;
    std::unordered_map<Entity, ComponentContainer> entities;
};

class Commands final {
public:
    Commands(World& world) : _world(world) {}

    template <typename... ComponentTypes>
    Commands& spawn(ComponentTypes&&... components) {
        Entity entity = EntityGenerator::Gen();
    }
private:
    World& _world;

    template <typename T, typename... Remains>
    void doSpawn(Entity entity, T&& components, Remains&&... remains) {
        auto index = IndexGetter::Get<T>();
        if (auto it = _world.entities.find(index); it == _world._componentMap.end()) {
            _world._componentMap.emplace(index);
        }   
        World::ComponentInfo& componentInfo = _world._componentMap[index];
        void* elem = componentInfo.pool.Create();

    }
};


}