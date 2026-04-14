#pragma once
#include <iostream>

namespace COMMON
{
	struct AsyncToken {
		std::atomic<bool> State{ false };
	};
}