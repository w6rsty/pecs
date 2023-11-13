#pragma once

namespace pecs {

class World {
private:


public:
    World& addStartupSystem();
    World& addSystem();
    World& addResource();

    void starup();
    void update();
    void shutdown();
};

}