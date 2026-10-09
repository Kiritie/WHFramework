#pragma once

#include "CoreMinimal.h"
#include "Containers/SparseSetElement.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"

class FVoxelTerrainResourceLease;

namespace VoxelBoundedAllocation
{
	/** 当前 allocator 的 native 容量上界，包含对齐慢路径；调用方须在分配前授予。 */
	WHFRAMEWORK_API uint64 QuantizeBytes(uint64 InBytes, uint32 InAlignment = DEFAULT_ALIGNMENT);

	template<typename FunctorType, bool bUnique = true>
	uint64 FunctionBytes()
	{
#if defined(NUM_TFUNCTION_INLINE_BYTES) && NUM_TFUNCTION_INLINE_BYTES > 0
		using FInlineObject = UE::Core::Private::Function::TFunction_OwnedObject<FunctorType, bUnique, false>;
		if constexpr (sizeof(FInlineObject) <= NUM_TFUNCTION_INLINE_BYTES)
		{
			return 0;
		}
		else
#endif
		{
			using FHeapObject = UE::Core::Private::Function::TFunction_OwnedObject<FunctorType, bUnique, true>;
			return QuantizeBytes(sizeof(FHeapObject), alignof(FHeapObject));
		}
	}

	template<typename TaskBodyType>
	uint64 TaskBytes()
	{
		using TaskType = UE::Tasks::Private::TExecutableTask<TaskBodyType>;
		return sizeof(TaskType) <= UE::Tasks::Private::SmallTaskSize ? UE::Tasks::Private::SmallTaskSize :
			QuantizeBytes(sizeof(TaskType), alignof(TaskType));
	}

	template<typename ElementType>
	uint64 ArrayBytes(const int32 InCount)
	{
		check(InCount >= 0);
		if (InCount == 0)
		{
			return 0;
		}
		FDefaultAllocator::ForElementType<ElementType> Allocator;
		int32 Capacity;
		if constexpr (TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment)
		{
			Capacity = Allocator.CalculateSlackReserve(InCount, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			Capacity = Allocator.CalculateSlackReserve(InCount, sizeof(ElementType));
		}
		check(Capacity >= InCount);
		return QuantizeBytes(uint64(Capacity) * sizeof(ElementType), TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment ? alignof(ElementType) : DEFAULT_ALIGNMENT);
	}

	template<typename ElementType>
	uint64 InitialGrowBytes(const int32 InCount)
	{
		check(InCount >= 0);
		if (InCount == 0) return 0;
		FDefaultAllocator::ForElementType<ElementType> Allocator;
		int32 Capacity;
		if constexpr (TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment)
		{
			Capacity = Allocator.CalculateSlackGrow(InCount, 0, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			Capacity = Allocator.CalculateSlackGrow(InCount, 0, sizeof(ElementType));
		}
		return QuantizeBytes(uint64(Capacity) * sizeof(ElementType), TAllocatorTraits<FDefaultAllocator>::SupportsElementAlignment ? alignof(ElementType) : DEFAULT_ALIGNMENT);
	}

	template<typename ElementType>
	bool Reserve(TArray<ElementType>& InArray, const int32 InCount, const uint64 InReservedBytes)
	{
		if (InCount < 0 || ArrayBytes<ElementType>(InCount) > InReservedBytes)
		{
			return false;
		}
		InArray.Reserve(InCount);
		return InArray.GetAllocatedSize() <= InReservedBytes;
	}

	// 与 UE 模块的全局 operator new 一致，小对象也使用默认 new 对齐而非仅类型对齐。
	template<typename ObjectType>
	uint64 NewBytes()
	{
		constexpr uint32 Alignment = alignof(ObjectType) > __STDCPP_DEFAULT_NEW_ALIGNMENT__ ?
			alignof(ObjectType) : sizeof(ObjectType) <= 8 ? 8 : __STDCPP_DEFAULT_NEW_ALIGNMENT__;
		return QuantizeBytes(sizeof(ObjectType), Alignment);
	}

	template<typename ObjectType>
	uint64 SharedBytes()
	{
		using FController = SharedPointerInternals::TIntrusiveReferenceController<ObjectType, ESPMode::ThreadSafe>;
		return NewBytes<FController>();
	}

	template<typename ObjectType>
	struct TWorkerDeleteTask
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		ObjectType* Object = nullptr;
		void operator()() const { delete Object; }
	};

	template<typename ObjectType, bool bAlwaysWorker = false>
	struct TWorkerDeleter
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		void operator()(ObjectType* Object) const
		{
			// 大输入、continuation 和其租约不能因 GT/RT 恰好释放最后一个引用而同步析构。
			if (bAlwaysWorker || IsInGameThread() || IsInRenderingThread())
				UE::Tasks::Launch(UE_SOURCE_LOCATION, TWorkerDeleteTask<ObjectType>{Lease, Object});
			else delete Object;
		}
	};

	template<typename ObjectType, bool bAlwaysWorker = false>
	uint64 WorkerSharedBytes()
	{
		using FController = SharedPointerInternals::TReferenceControllerWithDeleter<ObjectType,
			TWorkerDeleter<ObjectType, bAlwaysWorker>, ESPMode::ThreadSafe>;
		return NewBytes<ObjectType>() +
			NewBytes<FController>() +
			TaskBytes<TWorkerDeleteTask<ObjectType>>();
	}

	template<typename ObjectType, bool bAlwaysWorker = false, typename... ArgumentTypes>
	TSharedPtr<ObjectType, ESPMode::ThreadSafe> MakeWorkerShared(
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease, ArgumentTypes&&... InArguments)
	{
		check(InLease);
		return TSharedPtr<ObjectType, ESPMode::ThreadSafe>(new ObjectType(Forward<ArgumentTypes>(InArguments)...),
			TWorkerDeleter<ObjectType, bAlwaysWorker>{MoveTemp(InLease)});
	}

	// 覆盖 UE 5.8 位图首次与后续堆增长；调用方必须先 Reserve 再填充。
	template<typename ElementType>
	uint64 SetBytes(const int32 InCount)
	{
		check(InCount >= 0);
		if (InCount == 0) return 0;
#if UE_USE_COMPACT_SET_AS_DEFAULT
		return TSet<ElementType>::GetTotalMemoryRequiredInBytes(uint32(InCount));
#else
		using SparseElementType = TSparseSetElement<ElementType>;
		using StorageType = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(SparseElementType), alignof(SparseElementType)>>;
		const uint32 Words = (uint32(InCount) + 31) / 32;
		FDefaultAllocator::ForElementType<uint32> BitAllocator;
		const uint64 BitBytes = Words > 4 ? uint64(FMath::Max(
			BitAllocator.CalculateSlackGrow(int32(Words), 0, sizeof(uint32)),
			BitAllocator.CalculateSlackGrow(int32(Words), int32(Words) - 1, sizeof(uint32)))) * sizeof(uint32) : 0;
		const uint32 HashBuckets = FDefaultSetAllocator::GetNumberOfHashBuckets(uint32(InCount));
		return ArrayBytes<StorageType>(InCount) + QuantizeBytes(BitBytes) +
			(HashBuckets > 1 ? QuantizeBytes(uint64(HashBuckets) * sizeof(FSetElementId)) : 0);
#endif
	}

	template<typename KeyType, typename ValueType>
	uint64 MapBytes(const int32 InCount)
	{
		return SetBytes<TPair<KeyType, ValueType>>(InCount);
	}

	template<typename ElementType>
	uint64 SparseArrayBytes(const int32 InCount)
	{
		check(InCount >= 0);
		using StorageType = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(ElementType), alignof(ElementType)>>;
		const uint32 Words = (uint32(InCount) + 31) / 32;
		FDefaultAllocator::ForElementType<uint32> BitAllocator;
		const uint64 BitBytes = Words > 4 ? uint64(FMath::Max(
			BitAllocator.CalculateSlackGrow(int32(Words), 0, sizeof(uint32)),
			BitAllocator.CalculateSlackGrow(int32(Words), int32(Words) - 1, sizeof(uint32)))) * sizeof(uint32) : 0;
		return ArrayBytes<StorageType>(InCount) + QuantizeBytes(BitBytes);
	}
}
