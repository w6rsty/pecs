#include "config/config.hpp"
#include "entity/world.hpp"
#include "pecs.hpp"
#include <cstdio>
#include <string>
#include <iostream>

using namespace pecs;

struct Name {
    std::string name;
};

struct ID {
    unsigned int id;
};

struct Timer {
    int t;
};

void StartUpSystem(Commands command) {
    command.spawn<Name>(Name{"Mike"})
           .spawn<Name, ID>(Name{"Jack"}, {1})
           .spawn<ID>(ID{114})
           ;
}

void EchoNameSystem(Commands command, Queryer queryer, Resources resources) {
    std::cout << "<<Echo name system>>" << std::endl;
    auto entities = queryer.query<Name>();
    for (auto entity : entities) {
        std::cout << queryer.get<Name>(entity).name << std::endl;
    }
}

void EchoIDSystem(Commands command, Queryer queryer, Resources resources) {
    std::cout << "<<Echo ID system>>" << std::endl;
    auto entities = queryer.query<ID>();
    for (auto entity : entities) {
        std::cout << queryer.get<ID>(entity).id << std::endl;
    }
}

void EchoTimeSystem(Commands command, Queryer queryer, Resources resources) {
    std::cout << "<<Echo timer>>" << std::endl;
    auto resource = resources.get<Timer>();
    std::cout << resource.t << std::endl;
}

int main() {
    World world;
    world.addStartupSystem(StartUpSystem)
         .setResource<Timer>(Timer{0})
         .addSystem(EchoNameSystem)
         .addSystem(EchoIDSystem)
         .addSystem(EchoTimeSystem)
         ;

    world.startup();

    for (int i = 0 ; i < 3; i++) {
        world.update();
        std::cout  << "=================" << std::endl;
    }

    world.shutdown();
}
