#pragma once
#include <type_traits>
#include <vector>


namespace pecs {

template <typename T, size_t PageSize, typename = std::enable_if<std::is_integral_v<T>>>
class SparseSet final {
private:

public:
    void add() {

    }

    void remove() {

    }

    bool contain() {

    }

    void clear() {
        
    }
};

}