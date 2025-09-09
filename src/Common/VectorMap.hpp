#pragma once
#include <vector>
#include <unordered_map>
#include <iostream>

namespace COMMON
{
	template<typename Key,typename Value>
	class VectorMap
	{
	public:
		void Insert(const Key& Key,const Value& Value)
		{
			auto [Iterator, Inserted] = KeyIndexMap.emplace(Key, Data.size());
			if (!Inserted) return;
			Data.push_back({ Key,Value });
			Iterator->second = Data.size() - 1;
 		}

		std::pair<Key,Value>* Find(const Key& Key)
		{
			auto Iterator = KeyIndexMap.find(Key);
			if (Iterator == KeyIndexMap.end()) return nullptr;
			return &Data[Iterator->second];
		}

		void Erase(const Key& Key)
		{
			auto Iterator = KeyIndexMap.find(Key);
			if (Iterator == KeyIndexMap.end()) return;

			if (Iterator->second != (Data.size() - 1))
			{
				std::swap(Data[Iterator->second], Data.back());
				KeyIndexMap[Data[Iterator->second].first] = Iterator->second;
			}

			Data.pop_back();
			KeyIndexMap.erase(Iterator);
		}

		size_t Size()
		{
			return Data.size();
		}

		void Reserve(size_t NewCapacity) 
		{
			Data.reserve(NewCapacity);
			KeyIndexMap.reserve(NewCapacity);
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
	private:
		std::vector<std::pair<Key,Value>> Data;
		std::unordered_map<Key, size_t> KeyIndexMap;
	};
}
