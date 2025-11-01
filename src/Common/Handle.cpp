#include "Handle.hpp"

uint64_t COMMON::GenerateHandleID()
{
	static uint64_t Iterator = 1;
	return Iterator++;
}