#include "world.hpp"
#include <string>
#include <iostream>

using namespace pecs;

struct Player {};
struct Monster {};

struct Name {
    std::string name;
};

struct Position {
    float x, y;
};

std::ostream& operator<<(std::ostream& stream, const Position& pos) {
    stream << "(" <<  pos.x << ", " << pos.y << ")";
    return stream;
}

float distance(Position& p1, Position& p2) {
    return sqrt((p1.x - p2.x) * ( p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

struct HP {
    float value;
    float max = 100.0f;
};

struct Damage {
    float value;
};


void startup(Commands& command) {
    command.Spawn(Player{}, Name{"yuan_shen"}, Position{ .x = 1.0f, .y = 0.0f }, HP{ .value = 100.0f })
           .Spawn(Monster{}, Name{"w6rsty"}, Position{ .x = 1.0f, .y = 1.0f}, HP{ .value = 100.0f }, Damage{ .value = 20.0f});
}

void attackSystem(Commands& command, Queryer queryer, Resources resources, Events& events) {
    auto monsters = queryer.Query<Monster>();
    auto players  = queryer.Query<Player>();

    for (auto monster : monsters) {
        Position& p1 = queryer.Get<Position>(monster);
        for (auto player : players) {
            Position& p2 = queryer.Get<Position>(player);
        
            if (distance(p1, p2) <= 2.0f) {
                queryer.Get<HP>(player).value -= queryer.Get<Damage>(monster).value;
                std::cout << queryer.Get<Name>(monster).name << " 攻击了 " << queryer.Get<Name>(player).name << std::endl;
            }
        }
    }
}

void echoPlayerSystem(Commands& command, Queryer queryer, Resources resources, Events& events) {
    auto entities = queryer.Query<Player>();
    for (auto entity : entities) {
        std::cout << queryer.Get<Name>(entity).name << " | " 
        << queryer.Get<Position>(entity) << " | " 
        << "HP: "<< queryer.Get<HP>(entity).value << std::endl;
    }
}

void checkHPSystem(Commands& command, Queryer queryer, Resources resources, Events& events) {
    auto entities = queryer.Query<HP>();
    for (auto entity : entities) {
        auto hp = queryer.Get<HP>(entity).value;
        if (hp <= 0.0f) {
            std::cout << queryer.Get<Name>(entity).name << " dead" << std::endl;
            command.Destory(entity);
        }
    }
}



int main() {
    World world;
    world.AddStartupSystem(startup)
         .AddSystem(attackSystem)
         .AddSystem(echoPlayerSystem)
         .AddSystem(checkHPSystem)
         ;

    world.Startup();

    for (int i{0}; i < 6; i++) {
        world.Update();
        std::cout << "==============" << std::endl;
    }

    world.Shutdown();
}
