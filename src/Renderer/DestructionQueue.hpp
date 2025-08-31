#pragma once
#include <vector>

namespace COMMON
{
	class Destructible
	{
		friend class DestructionQueue;
	public:
		virtual ~Destructible() = default;
		virtual void Destroy();
	protected:
		bool IsDestroyed = true;
		uint16_t DestructionPriority;
	};

	class DestructionQueue
	{
	public:
		DestructionQueue() = default;
		~DestructionQueue();

		static DestructionQueue* Get();
		void Register(Destructible& DestructibleToRegister);
	private:
		std::vector<Destructible*> DestructibleList;
	};
}
