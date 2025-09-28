#pragma once
#include <vector>
#include <optional>
#include <iostream>

namespace COMMON
{
    template<typename T>
    class StableVector {
    public:
        StableVector(size_t ReserveCapacity = 100) {
            Data.reserve(ReserveCapacity);
            Alive.reserve(ReserveCapacity);
            FreeIndices.reserve(ReserveCapacity);
        }

        size_t push_back(T Input) {
            if (!FreeIndices.empty()) {
                size_t Index = FreeIndices.back();
                FreeIndices.pop_back();
                Data[Index] = std::move(Input);
                Alive[Index] = true;
                return Index;
            }
            Data.push_back(std::move(Input));
            Alive.push_back(true);
            return Data.size() - 1;
        }

        void erase(size_t Index) {
            Alive[Index] = false;
            FreeIndices.push_back(Index);
        }

        void Reserve(size_t ReserveSize)
        {
            Data.reserve(ReserveSize);
            Alive.reserve(ReserveSize);
            FreeIndices.reserve(ReserveSize);
        }

        bool valid(size_t Index) const {
            return Index < Alive.size() && Alive[Index];
        }

        T& operator[](size_t Index) { return Data[Index]; }
        const T& operator[](size_t Index) const { return Data[Index]; }

        auto& begin() noexcept { return Data.begin(); };
        auto& end() noexcept { return Data.end(); };
        size_t size() const { return Data.size(); }

    private:
        std::vector<T> Data;
        std::vector<bool> Alive;
        std::vector<size_t> FreeIndices;
    };
}