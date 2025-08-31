#pragma once
#include <vector>

namespace COMMON
{
	class Destructible
	{
		friend class DestructionQueue;
	public:
		virtual ~Destructible() = default;
		virtual void Destroy() = 0;
	protected:
		bool IsDestroyed = true;
		uint16_t DestructionPriority;
	};

	//Collects all of the destructibles and destroys them
	class DestructionQueue
	{
	public:
		DestructionQueue() = default;
		void Destroy();

		static DestructionQueue* Get();
		void Register(Destructible* DestructibleToRegister);
	private:
		std::vector<Destructible*> DestructibleList;
	};
}
