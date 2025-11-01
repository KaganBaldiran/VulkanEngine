#pragma once

namespace COMMON
{
	inline size_t CombineHash(const size_t& Hash0, const size_t& Hash1)
	{
		return Hash0 ^ (Hash1 + 0x9e3779b9 + (Hash0 << 6) + (Hash0 >> 2));
	}
	size_t HashNextPtrChain(void* Next);
}