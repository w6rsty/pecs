#pragma once
#include <array>
#include <limits>
#include <type_traits>
#include <memory>
#include <vector>

namespace pecs {

template <typename T, size_t PageSize, typename = std::enable_if<std::is_integral_v<T>>>
class SparseSet final {
private:
    std::vector<T> _packed;
    std::unique_ptr<std::array<T, PageSize>> _sparse;
    static constexpr T null = std::numeric_limits<T>::max();
    
     
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