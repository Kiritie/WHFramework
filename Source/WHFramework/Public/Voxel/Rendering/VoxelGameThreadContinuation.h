#pragma once

#include "CoreMinimal.h"
#include <coroutine>

// 每次 Resume 只推进到下一个微步；子任务保留自己的栈帧与输入生命期。
class FVoxelGameThreadContinuation
{
public:
	struct promise_type;
	using FHandle = std::coroutine_handle<promise_type>;

	struct promise_type
	{
		FHandle Child;

		FVoxelGameThreadContinuation get_return_object()
		{
			return FVoxelGameThreadContinuation(FHandle::from_promise(*this));
		}

		std::suspend_always initial_suspend() noexcept
		{
			return {};
		}

		std::suspend_always final_suspend() noexcept
		{
			return {};
		}

		void return_void() noexcept
		{
		}

		void unhandled_exception()
		{
			checkNoEntry();
		}
	};

	FVoxelGameThreadContinuation()
	{
	}
	FVoxelGameThreadContinuation(FVoxelGameThreadContinuation&& InOther) noexcept : Handle(InOther.Handle)
	{
		InOther.Handle = {};
	}

	FVoxelGameThreadContinuation& operator=(FVoxelGameThreadContinuation&& InOther) noexcept
	{
		if (this == &InOther)
		{
			return *this;
		}
		Reset();
		Handle = InOther.Handle;
		InOther.Handle = {};
		return *this;
	}

	~FVoxelGameThreadContinuation()
	{
		Reset();
	}
	FVoxelGameThreadContinuation(const FVoxelGameThreadContinuation&) = delete;
	FVoxelGameThreadContinuation& operator=(const FVoxelGameThreadContinuation&) = delete;

	bool IsValid() const
	{
		return bool(Handle);
	}
	bool IsComplete() const
	{
		return !Handle || Handle.done();
	}
	static std::suspend_always Yield() noexcept
	{
		return {};
	}

	void Reset()
	{
		if (Handle)
		{
			Handle.destroy();
		}
		Handle = {};
	}

	void Resume()
	{
		check(IsInGameThread());
		if (IsComplete())
		{
			return;
		}
		FHandle Next = Handle;
		while (Next.promise().Child)
		{
			if (Next.promise().Child.done())
			{
				Next.promise().Child = {};
				break;
			}
			Next = Next.promise().Child;
		}
		Next.resume();
	}

	WHFRAMEWORK_API void RunToCompletion();

	bool await_ready() const noexcept
	{
		return IsComplete();
	}
	void await_suspend(const FHandle InParent) noexcept
	{
		InParent.promise().Child = Handle;
	}

	void await_resume() const noexcept
	{
	}

private:
	explicit FVoxelGameThreadContinuation(const FHandle InHandle) : Handle(InHandle)
	{
	}
	FHandle Handle;
};

namespace VoxelGameThread
{
template <typename Element, typename Predicate> FVoxelGameThreadContinuation SortSteps(TArray<Element>& Values, Predicate Less)
{
	// 有界批次减少逐元素协程切换；排序顺序、完整元素数和调用者的截止时间不变。
	constexpr int32 ElementsPerStep = 32;
	TArray<Element> Buffer;
	Buffer.Reserve(Values.Num());
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		if (Index % ElementsPerStep == 0) co_await FVoxelGameThreadContinuation::Yield();
		Buffer.AddDefaulted();
	}
	for (int64 Width = 1; Width < Values.Num(); Width *= 2)
	{
		int32 MergedElements = 0;
		for (int64 Begin = 0; Begin < Values.Num(); Begin += Width * 2)
		{
			const int32 Middle = FMath::Min<int64>(Begin + Width, Values.Num());
			const int32 End = FMath::Min<int64>(Begin + Width * 2, Values.Num());
			int32 Left = Begin;
			int32 Right = Middle;
			for (int32 Out = Begin; Out < End; ++Out)
			{
				if (MergedElements++ % ElementsPerStep == 0) co_await FVoxelGameThreadContinuation::Yield();
				Buffer[Out] = Right >= End || (Left < Middle && !Less(Values[Right], Values[Left])) ? MoveTemp(Values[Left++]) : MoveTemp(Values[Right++]);
			}
		}
		Swap(Values, Buffer);
	}
}

template <typename Destination, typename Range> FVoxelGameThreadContinuation AppendSteps(Destination& To, const Range& From)
{
	for (const auto& Value : From)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		To.Add(Value);
	}
}
}
