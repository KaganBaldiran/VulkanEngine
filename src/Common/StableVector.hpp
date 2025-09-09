#pragma once
#include <vector>
#include <optional>
#include <iostream>

namespace COMMON
{
    template<typename T>
    class StableVectorAlive {
    public:
        StableVectorAlive(size_t reserveCapacity = 100) {
            Data.reserve(reserveCapacity);
            Alive.reserve(reserveCapacity);
            FreeIndices.reserve(reserveCapacity);
        }

        size_t push_back(T input) {
            if (!FreeIndices.empty()) {
                size_t idx = FreeIndices.back();
                FreeIndices.pop_back();
                Data[idx] = std::move(input);
                Alive[idx] = true;
                return idx;
            }
            Data.push_back(std::move(input));
            Alive.push_back(true);
            return Data.size() - 1;
        }

        void erase(size_t idx) {
            Alive[idx] = false;
            FreeIndices.push_back(idx);
        }

        bool valid(size_t idx) const {
            return idx < Alive.size() && Alive[idx];
        }

        T& operator[](size_t idx) { return Data[idx]; }
        const T& operator[](size_t idx) const { return Data[idx]; }

        size_t size() const { return Data.size(); }

    private:
        std::vector<T> Data;
        std::vector<bool> Alive;
        std::vector<size_t> FreeIndices;
    };
}