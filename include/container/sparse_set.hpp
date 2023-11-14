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
    std::vector<T> packed_;
    std::vector<std::unique_ptr<std::array<T, PageSize>>> sparse_;
    static constexpr T null = std::numeric_limits<T>::max();
public:
    void add(T t) {
        packed_.push_back(t);
        assure(t);
        index(t) = packed_.size() - 1;
    }   

    void remove(T t) {
        if (!contain(t)) return;

        auto& idx = index(t);
        if (idx == packed_.size() -1) {
            idx = null;
            packed_.pop_back();
        } else {
            auto last = packed_.back();
            index(last) = idx;
            std::swap(packed_[idx], packed_.back());
            idx = null;
            packed_.pop_back();
        }
    }

    bool contain(T t) {
        assert(t != null);

        auto p = page(t);
        auto o = offset(t);

        return p < sparse_.size() && sparse_[page(t)]->at(offset(t)) != null;
    }

    void clear() {
        packed_.clear();
        sparse_.clear();
    }

    auto begin() { return packed_.begin(); }
    auto end() { return packed_.end(); }
private:
    size_t page(T t) const {
        return t / PageSize;
    }
    
    size_t offset(T t) const {{
        return t % PageSize; 
    }} 

    T index(T t) const {
        return sparse_[page(t)]->at(offset(t));
    }

    T& index(T t) {
        return sparse_[page(t)]->at(offset(t));
    }

    void assure(T t) {
        auto p = page(t);
        if (p >= sparse_.size()) {
            for (size_t i = sparse_.size(); i <= p; i++) {
                sparse_.emplace_back(std::make_unique<std::array<T, PageSize>>());
                sparse_[i]->fill(null);
            }
        }
    }
};

}
