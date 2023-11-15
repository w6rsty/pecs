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

int main() {
    World world;
    Commands command(world);
    command.spawn<Name>(Name{"Hello"})
        .spawn<Name, ID>(Name{"Jack"}, {1})
        .setResource<Timer>(Timer{123});
}
