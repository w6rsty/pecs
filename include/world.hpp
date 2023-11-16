#pragma once
#include <vector>
#include <utility>
#include <cassert>
#include <algorithm>
#include <optional>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include "sparse_set.hpp"

#define PECS_SPARSE_PAGE 32
#define assertm(msg, expr) assert(((void)msg, (expr)))

namespace pecs {

using ComponentID = uint32_t;
using Entity = uint32_t;

struct Resource {};
struct Component {};

template <typename T>
class EventStaging final {
public:
    static bool Has() {
        return event_ != std::nullopt;
    }

    static void Set(T&& t) {
        event_ = std::move(t);
    }

    static void Set(const T& t) {
        event_ = t;
    }

    static T& Get() {
        return *event_;
    }

    static void Clear() {
        event_ = std::nullopt;
    }
private:
    inline static std::optional<T> event_ = std::nullopt;
};

template <typename T> 
class EventReader {
public:
  bool Has() {
    return EventStaging<T>::Has();
  }

  T Read(){
    return EventStaging<T>::Read();
  };

  void Clear() {
    EventStaging<T>::Clear();
  }
};

class Events final {
public:
    friend class World;

    template <typename T>
    friend class EventWriter;

    template <typename T>
    auto Reader();

    template <typename T>
    auto Writer();
private:
    std::vector<void(*)(void)> removeEventFuncs_;
    std::vector<void(*)(void)> removeOldEventFuncs_;
    std::vector<std::function<void(void)>> addEventFuncs_;
public:
    void addAllEvent() {
        for (auto func : addEventFuncs_) {
            func();
        }
        addEventFuncs_.clear();
    }

    void removeOldEvents() {
        for (auto func : removeOldEventFuncs_) {
            func();
        }
        removeOldEventFuncs_ = removeEventFuncs_;
        removeEventFuncs_.clear();
    }
};

template <typename T> 
class EventWriter {
public:
    EventWriter(Events& e) : events_(e) {}
    void Write(const T& t);
private:
    Events& events_;
};

template <typename T>
auto Events::Reader() {
    return EventReader<T>{};
}

template <typename T>
auto Events::Writer() {
    return EventWriter<T>{*this};
}

template <typename T>
void EventWriter<T>::Write(const T& t) {
    events_.addEventFuncs_.push_back([=]() {
        EventStaging<T>::Set(t);
    });
    events_.removeEventFuncs_.push_back([=]() {
        EventStaging<T>::Clear();
    });
}

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
class Events;

using UpdateSystem = void(*)(Commands&, Queryer, Resources, Events&);
using StartupSystem = void(*)(Commands&);

class World final {
public:
    friend class Commands;
    friend class Resources;
    friend class Queryer;
    using ComponentContainer = std::unordered_map<ComponentID, void*>;

    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    World& AddStartupSystem(StartupSystem sys) {
        startupSystems_.push_back(sys);
        return *this;
    }

    World& AddSystem(UpdateSystem sys) {
        updateSystems_.push_back(sys);
        return *this;
    }

    template <typename T>
    World& SetResource(T&& resource);

    void Startup();
    void Update();
    void Shutdown() {
        entities_.clear();
        resources_.clear();
        componentMap_.clear();
    }
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
    std::vector<StartupSystem> startupSystems_;
    std::vector<UpdateSystem> updateSystems_;
    Events events_;
};

class Commands final {
private:
    World& world_;

    using DestroyFunc = void(*)(void*);

    struct ResourceDestoryInfo {
        uint32_t index;
        DestroyFunc destroy;
        ResourceDestoryInfo(uint32_t index, DestroyFunc destroy) : index(index), destroy(destroy) {}
    };

    using AssignFunc = std::function<void(void*)>;

    struct ComponentSpawnInfo {
        AssignFunc assign;
        World::Pool::CreateFunc create;
        World::Pool::DestoryFunc destroy;
        ComponentID index;
    };

    struct EntitySpawnInfo {
        Entity entity; 
        std::vector<ComponentSpawnInfo> components; 
    };

    std::vector<Entity> destroyEntities_;
    std::vector<ResourceDestoryInfo> destoryResources_;
    std::vector<EntitySpawnInfo> spawnEntities_;
public:
    Commands(World& world) : world_(world) {}

    // 创建新的Entity
    template <typename... ComponentTypes>
    Commands& Spawn(ComponentTypes&&... components) {
        SpawnAndReturn<ComponentTypes...>(std::forward<ComponentTypes>(components)...);
        return *this;
    }

    template <typename... ComponentTypes>
    Entity SpawnAndReturn(ComponentTypes&&... components) {
        EntitySpawnInfo info;
        info.entity = EntityGenerator::Gen();
        doSpawn(info.entity, info.components, std::forward<ComponentTypes>(components)...);
        spawnEntities_.push_back(info);
        return info.entity;
    }

    Commands& Destory(Entity entity) {
        destroyEntities_.push_back(entity);
        return *this;
    }

    template <typename T>
    Commands& SetResource(T&& resource) {
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
    Commands& RemoveResource() {
        auto index = IndexGetter<Resource>::Get<T>();
        destoryResources_.push_back(ResourceDestoryInfo(index, [](void* elem) { delete (T*)elem; }));
        return *this;
    }

    void Execute() {
        for (auto& info : destoryResources_) {
            removeResource(info);
        }
        for (auto e : destroyEntities_) {
            destoryEntity(e);
        }

        for (auto& spawnInfo: spawnEntities_) {
            auto it = world_.entities_.emplace(spawnInfo.entity, World::ComponentContainer{});
            auto& componentContainer = it.first->second;
            for (auto& componentInfo : spawnInfo.components) {
                componentContainer[componentInfo.index] = doSpawnWithoutType(spawnInfo.entity, componentInfo);
            }
        }
    }

private:
    template <typename T, typename... Remains>
    void doSpawn(Entity entity, std::vector<ComponentSpawnInfo>& spawnInfos, T&& component, Remains&&... remains) {
        ComponentSpawnInfo info;
        info.index = IndexGetter<Component>::Get<T>();
        info.create = [](void)->void* { return new T; };
        info.destroy = [](void* elem) { delete (T*)elem; };
        info.assign = [=](void* elem) {
            *((T*)elem) = component;
        };
        spawnInfos.push_back(info);

        if constexpr (sizeof...(Remains) != 0) {
            doSpawn<Remains...>(entity, spawnInfos, std::forward<Remains>(remains)...);
        }
    }   

    void* doSpawnWithoutType(Entity entity, ComponentSpawnInfo& info) {
        if (auto it = world_.componentMap_.find(info.index); it == world_.componentMap_.end()) {
            world_.componentMap_.emplace(info.index, World::ComponentInfo(info.create, info.destroy));
        }
        World::ComponentInfo& componentInfo = world_.componentMap_[info.index];
        void* elem = componentInfo.pool.Create();
        info.assign(elem);
        componentInfo.sparseSet.add(entity);
        return elem;
    }


    void destoryEntity(Entity entity) {
        if (auto it = world_.entities_.find(entity); it != world_.entities_.end()) {
            for (auto [id, component] : it->second) {
                auto& componentInfo = world_.componentMap_[id];
                componentInfo.pool.Destory(component);
                componentInfo.sparseSet.remove(entity);
            }
            world_.entities_.erase(it);
        }
    }

    void removeResource(ResourceDestoryInfo& info) {
        if (auto it = world_.resources_.find(info.index); it != world_.resources_.end()) {
            info.destroy(it->second.resource);
            it->second.resource = nullptr;
        }
    }
};

class Resources final {
private:
    World& world_;
public:
    Resources(World& world) : world_(world) {}

    template <typename T>
    bool Has() const {
        auto index = IndexGetter<Resource>::Get<T>();
        auto it = world_.resources_.find(index);
        return it != world_.resources_.end() && it->second.resource;
    }

    template <typename T>
    T& Get() {
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
    std::vector<Entity> Query() {
        std::vector<Entity> entities;
        doQuery<Components...>(entities);
        return entities;
    }

    template <typename T>
    bool Has(Entity entity) {
        auto it = world_.entities_.find(entity);
        auto index = IndexGetter<Component>::Get<T>();
        return it != world_.entities_.end() && it->second.find(index) != it->second.end();
    }

    template <typename T>
    T& Get(Entity entity) {
        auto index = IndexGetter<Component>::Get<T>();
        return *((T*)world_.entities_[entity][index]);
    }
private:
    template <typename T, typename... Remains>
    void doQuery(std::vector<Entity>& outEntities) {
        auto index = IndexGetter<Component>::Get<T>();
        World::ComponentInfo& info  = world_.componentMap_[index];

        for (auto e : info.sparseSet) {
            if constexpr (sizeof...(Remains) != 0) {
                doQueryRemains<Remains...>(e, outEntities);
            } else {
                outEntities.push_back(e);
            }
        }
    }

    template <typename T, typename... Remains>
    void doQueryRemains(Entity entity, std::vector<Entity>& outEntites) {
        auto index = IndexGetter<Component>::Get<T>();
        auto& componentContainer = world_.entities_[entity];
        if (auto it = componentContainer.find(index); it == componentContainer.end()) {
            return;
        }

        if constexpr (sizeof...(Remains) == 0) {
            outEntites.push_back(entity);
        } else {
            doQueryRemains<Remains...>(entity, outEntites);
        }
    }
};

inline void World::Startup() {
    std::vector<Commands> commandList;
    
    for (auto sys : startupSystems_) {
        Commands command{*this};
        sys(command);
        commandList.push_back(command);
    }
 
    for (auto& command : commandList) {
        command.Execute();
    }
}

inline void World::Update() {
    std::vector<Commands> commandList;

    for (auto sys : updateSystems_) {
        Commands command{*this};
        sys(command, Queryer{*this}, Resources{*this}, events_);
        commandList.push_back(command);
    }
    events_.removeOldEvents();
    events_.addAllEvent();

    for (auto& command : commandList) {
        command.Execute();
    }
}

template <typename T>
World& World::SetResource(T &&resource) {
    Commands command(*this);
    command.SetResource(std::forward<T>(resource));

    return *this;
}

}