#pragma once
#include <iostream>

namespace COMMON
{
	template<typename CountType>
	struct Referenceable 
	{
		CountType ReferenceCount = 0;
		CountType IncreaseReference() 
		{ 
			ReferenceCount++; 
			return ReferenceCount;
		}
		CountType DecreaseReference() 
		{
			if (ReferenceCount) ReferenceCount--; 
			return ReferenceCount;
		}
		bool IsReferenced() const { return ReferenceCount; }
	};
}