#pragma once
#include "../pecs.hpp"
#include "container/sparse_set.hpp"
#include "entity/world.hpp"
#include <errno.h>
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
private:
    inline static ComponentID curIdx_ = {};
public:
    template <typename T>
    static ComponentID Get() { return curIdx_++; }
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

class World final {
public:
    friend class Commands;
    using ComponentContainer = std::unordered_map<ComponentID, void*>;
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
};

class Commands final {
private:
    World& world_;
public:
    Commands(World& world) : world_(world) {}

    // 创建新的Entity
    template <typename... ComponentTypes>
    Commands& spawn(ComponentTypes&&... components) {
        Entity entity = EntityGenerator::Gen();
        doSpawn(entity, std::forward<ComponentTypes>(components)...);
        return *this;
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
    
private:
    template <typename T, typename... Remains>
    void doSpawn(Entity entity, T&& component, Remains&&... remains) {
        auto index = IndexGetter::Get<T>(); // 每种Component都会具有单独的Index计数器
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

}