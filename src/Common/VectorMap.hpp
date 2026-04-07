#pragma once
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>

namespace COMMON
{
	template<typename Key,typename Value,typename Hash = std::hash<Key>>
	class VectorMap
	{
	public:
		void insert(const Key& Key,const Value& Value)
		{
			auto [Iterator, Inserted] = KeyIndexMap.emplace(Key, Data.size());
			if (!Inserted) return;
			Data.push_back({ Key,Value });
			Iterator->second = Data.size() - 1;
 		}

		std::pair<Key,Value>* find(const Key& Key)
		{
			auto Iterator = KeyIndexMap.find(Key);
			if (Iterator == KeyIndexMap.end()) return nullptr;
			return &Data[Iterator->second];
		}

		void erase(const Key& Key)
		{
			auto Iterator = KeyIndexMap.find(Key);
			if (Iterator == KeyIndexMap.end()) return;

			auto& Index = Iterator->second;
			if (Iterator->second != (Data.size() - 1))
			{
				std::swap(Data[Index], Data.back());
				KeyIndexMap[Data[Index].first] = Index;
			}

			Data.pop_back();
			KeyIndexMap.erase(Iterator);
		}

		size_t size()
		{
			return Data.size();
		}

		void reserve(size_t NewCapacity) 
		{
			Data.reserve(NewCapacity);
			KeyIndexMap.reserve(NewCapacity);
		}

		void clear()
		{
			Data.clear();
			KeyIndexMap.clear();
		}

		template <typename CompareFunc>
		void sort(CompareFunc CompareFunction)
		{
			std::sort(Data.begin(), Data.end(), CompareFunction);
			for (size_t i = 0; i < Data.size(); i++)
			{
				auto& CurrentData = Data[i];
				KeyIndexMap[CurrentData.first] = i;
			}
		}

		std::pair<Key, Value>& operator[](const Key& Key)
		{
			return Data[KeyIndexMap[Key]];
		}

		const std::pair<Key, Value>& operator[](const Key& Key) const 
		{
			return Data[KeyIndexMap[Key]];
		}

		auto begin() { return Data.begin(); };
		auto end() { return Data.end(); };
		std::vector<std::pair<Key, Value>>& getData() { return Data; };
	private:
		std::vector<std::pair<Key,Value>> Data;
		std::unordered_map<Key, size_t, Hash> KeyIndexMap;
	};
}
