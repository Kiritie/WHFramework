#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Templates/Atomic.h"
#include "Templates/SharedPointer.h"

// 节点内嵌不可变资源的已授予元数据；Payload 必须由调用者采用固定容量布局。
template<typename OwnerType, typename PayloadType>
struct TVoxelCoalescedSubmissionNode
{
	mutable FCriticalSection Mutex;
	TSharedPtr<const OwnerType, ESPMode::ThreadSafe> Owner;
	PayloadType Payload{};
	TVoxelCoalescedSubmissionNode* Next = nullptr;
	bool bQueued = false;
};

// 多生产者、单 GT 消费者。一个资源至多挂一个槽；消费批次只移动链头，不重建队列。
template<typename OwnerType, typename PayloadType>
class TVoxelCoalescedSubmissionMailbox
{
public:
	using FNode = TVoxelCoalescedSubmissionNode<OwnerType, PayloadType>;
	using FOwner = TSharedPtr<const OwnerType, ESPMode::ThreadSafe>;

	~TVoxelCoalescedSubmissionMailbox()
	{
		check(!Consuming && !Head.Load());
	}

	void Publish(FOwner InOwner, FNode& InNode, TFunctionRef<void(PayloadType&)> InUpdate)
	{
		check(InOwner);
		FScopeLock Lock(&InNode.Mutex);
		check(!InNode.bQueued || InNode.Owner == InOwner);
		InUpdate(InNode.Payload);
		if (InNode.bQueued)
		{
			return;
		}
		InNode.Owner = MoveTemp(InOwner);
		InNode.bQueued = true;
		FNode* Previous = Head.Load();
		do
		{
			InNode.Next = Previous;
		}
		while (!Head.CompareExchange(Previous, &InNode));
	}

	bool TryConsume(FOwner& OutOwner, PayloadType& OutPayload)
	{
		check(IsInGameThread());
		if (!Consuming)
		{
			Consuming = Head.Exchange(nullptr);
		}
		if (!Consuming || !Consuming->Mutex.TryLock())
		{
			return false;
		}
		FNode* Node = Consuming;
		Consuming = Node->Next;
		OutOwner = MoveTemp(Node->Owner);
		OutPayload = MoveTemp(Node->Payload);
		Node->Payload = PayloadType();
		Node->Next = nullptr;
		Node->bQueued = false;
		Node->Mutex.Unlock();
		return true;
	}

	bool IsEmpty() const
	{
		check(IsInGameThread());
		return !Consuming && !Head.Load();
	}

private:
	TAtomic<FNode*> Head{nullptr};
	FNode* Consuming = nullptr;
};
