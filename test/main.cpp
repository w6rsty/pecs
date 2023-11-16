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

void startup(Commands& command) {
    command.Spawn(Player{}, Name{"w6rsty"}, Position{ .x = 0.0f, .y = 0.0f }, HP{ .value = 100.0f })
           .Spawn(Monster{}, Name{"XXX"}, Position{ .x = 1.0f, .y = 1.0f}, HP{ .value = 100.0f});
}

void attackSystem(Commands& command, Queryer queryer, Resources resources, Events& events) {
    auto monsters = queryer.Query<Monster>();
    auto players  = queryer.Query<Player>();

    for (auto player : players) {
        Position& p1 = queryer.Get<Position>(player);
        for (auto monster : monsters) {
            Position& p2 = queryer.Get<Position>(monster);

            if (distance(p1, p2) <= 2.0f) {
                auto& hp = queryer.Get<HP>(player);
                hp.value -= 10.f;
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

void echoHPSystem(Commands& command, Queryer queryer, Resources resources, Events& events) {
    auto entities = queryer.Query<HP>();
    for (auto entity : entities) {
        
        std::cout << queryer.Get<Name>(entity).name << " HP: " <<queryer.Get<HP>(entity).value << std::endl;
    }
}


int main() {
    World world;
    world.AddStartupSystem(startup)
         .AddSystem(attackSystem)
         .AddSystem(echoPlayerSystem);

    world.Startup();

    for (int i{0}; i < 3; i++) {
        world.Update();
    }

    world.Shutdown();
}
