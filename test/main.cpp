#include "config/config.hpp"
#include "entity/world.hpp"
#include "pecs.hpp"
#include <cstdio>
#include <string>

using namespace pecs;

struct Name {
    std::string name;
};

struct ID {
    unsigned int id;
};

struct Timer {
    int timer;
};

int main() {
    World world;
    Commands command(world);
    command.spawn<Name>(Name{"Hello"})
           .setResource<Timer>(Timer{1})
           .removeResource<Timer>();
}