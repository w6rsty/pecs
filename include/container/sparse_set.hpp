#pragma once
#include <type_traits>
#include <array>
#include <memory>
#include <vector>
#include <cassert>

namespace pecs {

template <typename T, size_t PageSize, typename = std::enable_if<std::is_integral_v<T>>>
class SparseSet final {
private:
public:
    std::vector<T> _packed;
    std::vector<std::unique_ptr<std::array<T, PageSize>>> _sparse;
    static constexpr T null = std::numeric_limits<T>::max();
public:
    void add(T t) {
        _packed.push_back(t);
        assure(t);
        index(t) = _packed.size() - 1;
    }   

    void remove(T t) {
        if (!contain(t)) return;

        auto& idx = index(t);
        if (idx == _packed.size() -1) {
            idx = null;
            _packed.pop_back();
        } else {
            auto last = _packed.back();
            index(last) = idx;
            std::swap(_packed[idx], _packed.back());
            idx = null;
            _packed.pop_back();
        }
    }

    bool contain(T t) {
        assert(t != null);

        auto p = page(t);
        auto o = offset(t);

        return p < _sparse.size() && _sparse[page(t)]->at(offset(t)) != null;
    }

    void clear() {
        _packed.clear();
        _sparse.clear();
    }

    auto begin() { return _packed.begin(); }
    auto end() { return _packed.end(); }
private:
    size_t page(T t) const {
        return t / PageSize;
    }
    
    size_t offset(T t) const {{
        return t % PageSize; 
    }} 

    T index(T t) const {
        return _sparse[page(t)]->at(offset(t));
    }

    T& index(T t) {
        return _sparse[page(t)]->at(offset(t));
    }

    void assure(T t) {
        auto p = page(t);
        if (p >= _sparse.size()) {
            for (size_t i = _sparse.size(); i <= p; i++) {
                _sparse.emplace_back(std::make_unique<std::array<T, PageSize>>());
                _sparse[i]->fill(null);
            }
        }
    }
};

}
