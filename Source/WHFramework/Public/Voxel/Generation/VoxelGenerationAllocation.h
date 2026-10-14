#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

class FVoxelGenerationWorkspace;
class FVoxelGenerationAllocationScope;

/** 当前调用强持的原生来源；同原账本 serial 或 immutable 地址只计一次。 */
struct WHFRAMEWORK_API FVoxelGenerationRequiredSource
{
	const void* Object = nullptr;
	uint64 Serial = 0;
	EVoxelTerrainResourcePool Pool = EVoxelTerrainResourcePool::Input;
	uint64 Bytes = 0;
	int32 LeaseCount = 1;
	// 先按身份去重，首次计入时才从原账本读取同一时刻的 pool/bytes。
	const FVoxelTerrainResourceLease* Lease = nullptr;
};

struct WHFRAMEWORK_API FVoxelGenerationRequiredCapacity
{
	uint64 Bytes[static_cast<int32>(EVoxelTerrainResourcePool::Count)] = {};
	int32 LeaseCount = 0;
};

struct FVoxelGenerationSourceIdentity
{
	const void* Object = nullptr;
	uint64 Serial = 0;
};

/** 栈上来源链，不分配 tracking backing；来源的原持有者必须覆盖整个 scope。 */
class WHFRAMEWORK_API FVoxelGenerationRequiredSourceScope
{
public:
	using FVisit = TFunctionRef<void(const FVoxelGenerationRequiredSource&)>;
	using FEnumerate = void (*)(const void*, FVisit);

	FVoxelGenerationRequiredSourceScope(
		const FVoxelTerrainResources* InResources,
		const void* InContext,
		FEnumerate InEnumerate);
	FVoxelGenerationRequiredSourceScope(
		const FVoxelTerrainResources* InResources,
		const FVoxelTerrainResourceLease* InLease);
	~FVoxelGenerationRequiredSourceScope();
	FVoxelGenerationRequiredSourceScope(const FVoxelGenerationRequiredSourceScope&) = delete;
	FVoxelGenerationRequiredSourceScope& operator=(const FVoxelGenerationRequiredSourceScope&) = delete;

	static void VisitLease(const FVoxelTerrainResourceLease* InLease, FVisit InVisit);
	static bool Measure(
		const void* InContext,
		FEnumerate InEnumerate,
		FVoxelGenerationRequiredCapacity& OutCapacity,
		FString& OutError);
	static bool Collect(
		const FVoxelTerrainResources& InResources,
		const FVoxelTerrainResourceLease* InExcludedLease,
		FVoxelGenerationRequiredCapacity& OutCapacity,
		FString& OutError,
		TArrayView<FVoxelGenerationSourceIdentity> InIdentities = {});

private:
	static bool MeasureSources(TFunctionRef<void(FVisit)> InEnumerate, uint64 InExcludedSerial,
		FVoxelGenerationRequiredCapacity& OutCapacity, FString& OutError,
		TArrayView<FVoxelGenerationSourceIdentity> InIdentities = {});
	static void Enumerate(const FVoxelTerrainResources& InResources, FVisit InVisit);
	static FVoxelGenerationRequiredSourceScope*& ActiveScopeSlot();
	FVoxelGenerationRequiredSourceScope* Previous = nullptr;
	const FVoxelTerrainResources* Resources = nullptr;
	const void* Context = nullptr;
	FEnumerate Enumerator = nullptr;
};

/** 原账本的正文构建上下文；只保存聚合字节，不另建容器跟踪表。 */
class WHFRAMEWORK_API FVoxelGenerationAllocationScope
{
public:
	FVoxelGenerationAllocationScope(
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InWorkingLease,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InResidentReservation,
		uint64 InRootBytes,
		// 恢复只接受原所有者实际保留的正文容量，不接受估算值。
		uint64 InRetainedBytes = 0);
	~FVoxelGenerationAllocationScope();
	FVoxelGenerationAllocationScope(const FVoxelGenerationAllocationScope&) = delete;
	FVoxelGenerationAllocationScope& operator=(const FVoxelGenerationAllocationScope&) = delete;

	static FVoxelGenerationAllocationScope* Get();
	static bool AllowPassive(FString& OutError);
	bool IsBound() const;
	bool Check(FString& OutError) const;
	bool Fail(const TCHAR* InError, FString& OutError);
	bool PrepareBytes(uint64 InOldBytes, uint64 InNewBytes, FString& OutError);
	bool SettleBytes(uint64 InOldBytes, uint64 InReservedBytes, uint64 InActualBytes, FString& OutError);
	// 对应 native backing 必须先释放；Reset 保留 backing，不得退租。
	void ReleaseBytes(uint64 InBytes);
	void ReleaseRetainedBytes(uint64 InBytes);
	bool ValidateRetainedBytes(uint64 InActualBytes, FString& OutError);

private:
	friend class FVoxelGenerationWorkspace;
	friend class FVoxelGenerationRequiredSourceScope;
	static FVoxelGenerationAllocationScope*& ActiveScopeSlot();
	FVoxelGenerationAllocationScope* Previous = nullptr;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentReservation;
	uint64 RootBytes = 0;
	TCHAR Error[1024] = {};
};

namespace VoxelGenerationAllocation
{
	namespace Private
	{
		static_assert(!UE_ENABLE_ARRAY_SLACK_TRACKING, "Native generation allocation requires explicit slack tracking header accounting");

		/** 固定栈 archive 读取 UE 5.8 sparse set 的三块容量，无动态跟踪元数据。 */
		class WHFRAMEWORK_API FSetCapacityArchive final : public FArchive
		{
		public:
			FSetCapacityArchive();
			~FSetCapacityArchive();
			virtual void CountBytes(SIZE_T InNum, SIZE_T InMax) override;
			uint64 MaxBytes[3] = {};
			int32 Count = 0;
		};

		template<typename AllocatorType>
		struct TSupportedArrayAllocator : std::false_type
		{
		};

		template<>
		struct TSupportedArrayAllocator<FDefaultAllocator> : std::true_type
		{
		};

		template<uint32 NumInlineElements>
		struct TSupportedArrayAllocator<TInlineAllocator<NumInlineElements, FDefaultAllocator>> : std::true_type
		{
		};

		// 只开放已按真实 allocator 核对的策略，固定或其它自定义策略不能套用 default 估值。
		template<typename ElementType, typename AllocatorType>
		uint64 ArrayReserveBytes(const int32 InCapacity)
		{
			static_assert(TSupportedArrayAllocator<AllocatorType>::value);
			using FAllocator = typename AllocatorType::template ForElementType<ElementType>;
			FAllocator Allocator;
			const auto Capacity = [&Allocator, InCapacity]()
			{
				if constexpr (TAllocatorTraits<AllocatorType>::SupportsElementAlignment)
				{
					return Allocator.CalculateSlackReserve(InCapacity, sizeof(ElementType), alignof(ElementType));
				}
				else
				{
					return Allocator.CalculateSlackReserve(InCapacity, sizeof(ElementType));
				}
			}();
			const uint64 RawBytes = Allocator.GetAllocatedSize(Capacity, sizeof(ElementType));
			constexpr uint32 Alignment = TAllocatorTraits<AllocatorType>::SupportsElementAlignment ? alignof(ElementType) : DEFAULT_ALIGNMENT;
			return RawBytes ? VoxelBoundedAllocation::QuantizeBytes(RawBytes, Alignment) : 0;
		}

		template<typename ContainerType>
		void ReadSetCapacity(const ContainerType& InContainer, FSetCapacityArchive& OutCapacity)
		{
			InContainer.CountBytes(OutCapacity);
			check(OutCapacity.Count == 3);
		}

		template<typename ElementType, typename ContainerType>
		bool SetCanReserve(const ContainerType& InContainer, const int32 InCapacity)
		{
#if UE_USE_COMPACT_SET_AS_DEFAULT
			return InCapacity <= InContainer.Max();
#else
			using FSparseElement = TSparseSetElement<ElementType>;
			using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(FSparseElement), alignof(FSparseElement)>>;
			FSetCapacityArchive Capacity;
			ReadSetCapacity(InContainer, Capacity);
			const uint64 FlagBytes = ((uint64(InCapacity) + 31) / 32) * sizeof(uint32);
			const uint64 HashBytes = uint64(FDefaultSetAllocator::GetNumberOfHashBuckets(InCapacity)) * sizeof(FSetElementId);
			// 一桶 hash 在对象内；创建它不分配 heap。
			return uint64(InCapacity) * sizeof(FStorage) <= Capacity.MaxBytes[0] &&
				FlagBytes <= Capacity.MaxBytes[1] && HashBytes <= FMath::Max<uint64>(sizeof(FSetElementId), Capacity.MaxBytes[2]);
#endif
		}

		template<typename ElementType, typename ContainerType>
		uint64 SetAllocatedBytes(const ContainerType& InContainer)
		{
#if UE_USE_COMPACT_SET_AS_DEFAULT
			const uint64 RawBytes = InContainer.GetAllocatedSize();
			return RawBytes ? VoxelBoundedAllocation::QuantizeBytes(RawBytes, FMath::Max<uint32>(alignof(ElementType), sizeof(uint32))) : 0;
#else
			using FSparseElement = TSparseSetElement<ElementType>;
			using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(FSparseElement), alignof(FSparseElement)>>;
			FSetCapacityArchive Capacity;
			ReadSetCapacity(InContainer, Capacity);
			const uint64 Data = Capacity.MaxBytes[0] ? VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[0], alignof(FStorage)) : 0;
			const uint64 Flags = Capacity.MaxBytes[1] > 4 * sizeof(uint32) ? VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[1], DEFAULT_ALIGNMENT) : 0;
			const uint64 Hash = Capacity.MaxBytes[2] > sizeof(FSetElementId) ? VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[2], DEFAULT_ALIGNMENT) : 0;
			return Data + Flags + Hash;
#endif
		}

		template<typename ElementType, typename ContainerType>
		uint64 SetReserveBytes(const ContainerType& InContainer, const int32 InCapacity)
		{
#if UE_USE_COMPACT_SET_AS_DEFAULT
			const uint64 RawBytes = TSet<ElementType>::GetTotalMemoryRequiredInBytes(uint32(InCapacity));
			if (RawBytes > MAX_int32)
			{
				return MAX_uint64;
			}
			return FMath::Max(SetAllocatedBytes<ElementType>(InContainer),
				uint64(VoxelBoundedAllocation::QuantizeBytes(RawBytes, FMath::Max<uint32>(alignof(ElementType), sizeof(uint32)))));
#else
			using FSparseElement = TSparseSetElement<ElementType>;
			using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(FSparseElement), alignof(FSparseElement)>>;
			FSetCapacityArchive Capacity;
			ReadSetCapacity(InContainer, Capacity);
			const uint64 Data = FMath::Max(
				Capacity.MaxBytes[0] ? uint64(VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[0], alignof(FStorage))) : 0,
				ArrayReserveBytes<FStorage, FDefaultAllocator>(InCapacity));
			const int32 Words = int32((uint64(InCapacity) + 31) / 32);
			const int32 OldWords = int32(Capacity.MaxBytes[1] / sizeof(uint32));
			FDefaultBitArrayAllocator::ForElementType<uint32> Allocator;
			const uint64 FlagRaw = Words > OldWords ? uint64(Allocator.CalculateSlackGrow(Words, OldWords, sizeof(uint32))) * sizeof(uint32) : Capacity.MaxBytes[1];
			if (FlagRaw / sizeof(uint32) > uint64(MAX_int32) / 32)
			{
				return MAX_uint64;
			}
			const uint64 Flags = FlagRaw > 4 * sizeof(uint32) ? VoxelBoundedAllocation::QuantizeBytes(FlagRaw, DEFAULT_ALIGNMENT) : 0;
			const uint64 HashRaw = FMath::Max(Capacity.MaxBytes[2], uint64(FDefaultSetAllocator::GetNumberOfHashBuckets(InCapacity)) * sizeof(FSetElementId));
			const uint64 Hash = HashRaw > sizeof(FSetElementId) ? VoxelBoundedAllocation::QuantizeBytes(HashRaw, DEFAULT_ALIGNMENT) : 0;
			return Data + Flags + Hash;
#endif
		}

		template<typename ElementType>
		uint64 SparseArrayReserveBytes(const TSparseArray<ElementType>& InArray, const int32 InCapacity)
		{
			using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(ElementType), alignof(ElementType)>>;
			FSetCapacityArchive Capacity;
			InArray.CountBytes(Capacity);
			check(Capacity.Count == 2);
			const uint64 Data = FMath::Max(
				Capacity.MaxBytes[0] ? uint64(VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[0], alignof(FStorage))) : 0,
				ArrayReserveBytes<FStorage, FDefaultAllocator>(InCapacity));
			const int32 Words = int32((uint64(InCapacity) + 31) / 32);
			const int32 OldWords = int32(Capacity.MaxBytes[1] / sizeof(uint32));
			FDefaultBitArrayAllocator::ForElementType<uint32> Allocator;
			const uint64 FlagRaw = Words > OldWords ?
				uint64(Allocator.CalculateSlackGrow(Words, OldWords, sizeof(uint32))) * sizeof(uint32) : Capacity.MaxBytes[1];
			return Data + (FlagRaw > 4 * sizeof(uint32) ? VoxelBoundedAllocation::QuantizeBytes(FlagRaw, DEFAULT_ALIGNMENT) : 0);
		}
	}

	template<typename ElementType, typename AllocatorType>
	uint64 AllocatedBytes(const TArray<ElementType, AllocatorType>& InArray)
	{
		static_assert(Private::TSupportedArrayAllocator<AllocatorType>::value);
		constexpr uint32 Alignment = TAllocatorTraits<AllocatorType>::SupportsElementAlignment ? alignof(ElementType) : DEFAULT_ALIGNMENT;
		const uint64 RawBytes = InArray.GetAllocatedSize();
		return RawBytes ? VoxelBoundedAllocation::QuantizeBytes(RawBytes, Alignment) : 0;
	}

	template<typename ElementType>
	uint64 AllocatedBytes(const TSparseArray<ElementType>& InArray)
	{
		using FStorage = TSparseArrayElementOrFreeListLink<TAlignedBytes<sizeof(ElementType), alignof(ElementType)>>;
		Private::FSetCapacityArchive Capacity;
		InArray.CountBytes(Capacity);
		check(Capacity.Count == 2);
		const uint64 Data = Capacity.MaxBytes[0] ? VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[0], alignof(FStorage)) : 0;
		const uint64 Flags = Capacity.MaxBytes[1] > 4 * sizeof(uint32) ? VoxelBoundedAllocation::QuantizeBytes(Capacity.MaxBytes[1], DEFAULT_ALIGNMENT) : 0;
		return Data + Flags;
	}

	// Map/Set 仅支持原 default allocator；自定义 KeyFuncs 不改变 allocation 口径。
	template<typename KeyType, typename ValueType, typename KeyFuncs>
	uint64 AllocatedBytes(const TMap<KeyType, ValueType, FDefaultSetAllocator, KeyFuncs>& InMap)
	{
		return Private::SetAllocatedBytes<TPair<KeyType, ValueType>>(InMap);
	}

	template<typename ElementType, typename KeyFuncs>
	uint64 AllocatedBytes(const TSet<ElementType, KeyFuncs, FDefaultSetAllocator>& InSet)
	{
		return Private::SetAllocatedBytes<ElementType>(InSet);
	}

	namespace Private
	{
		template<typename BudgetType, typename ElementType, typename AllocatorType>
		bool ReserveArray(BudgetType* InBudget, TArray<ElementType, AllocatorType>& InArray, const int32 InCapacity, FString& OutError)
		{
			if (!(InBudget ? InBudget->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
			{
				return false;
			}
			if (InCapacity < 0)
			{
				OutError = TEXT("Oversized: negative native array capacity");
				return InBudget ? InBudget->Fail(*OutError, OutError) : false;
			}
			if (InCapacity <= InArray.Max())
			{
				return true;
			}
			const uint64 OldBytes = AllocatedBytes(InArray);
			const uint64 NewBytes = ArrayReserveBytes<ElementType, AllocatorType>(InCapacity);
			if (InBudget && !InBudget->PrepareBytes(OldBytes, NewBytes, OutError))
			{
				return false;
			}
			InArray.Reserve(InCapacity);
			return !InBudget || InBudget->SettleBytes(OldBytes, NewBytes, AllocatedBytes(InArray), OutError);
		}

		template<typename ElementType, typename BudgetType, typename ContainerType>
		bool ReserveSet(BudgetType* InBudget, ContainerType& InContainer, const int32 InCapacity, FString& OutError)
		{
			if (!(InBudget ? InBudget->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
			{
				return false;
			}
			if (InCapacity < 0)
			{
				OutError = TEXT("Oversized: negative native set capacity");
				return InBudget ? InBudget->Fail(*OutError, OutError) : false;
			}
			if (SetCanReserve<ElementType>(InContainer, InCapacity))
			{
				return true;
			}
			const uint64 OldBytes = AllocatedBytes(InContainer);
			const uint64 NewBytes = SetReserveBytes<ElementType>(InContainer, InCapacity);
			if (NewBytes == MAX_uint64)
			{
				OutError = TEXT("Oversized: native set allocator capacity");
				return InBudget ? InBudget->Fail(*OutError, OutError) : false;
			}
			if (InBudget && !InBudget->PrepareBytes(OldBytes, NewBytes, OutError))
			{
				return false;
			}
			InContainer.Reserve(InCapacity);
			return !InBudget || InBudget->SettleBytes(OldBytes, NewBytes, AllocatedBytes(InContainer), OutError);
		}

		template<typename BudgetType, typename ContainerType>
		void FreeContainer(BudgetType* InBudget, ContainerType& InContainer)
		{
			const uint64 OldBytes = AllocatedBytes(InContainer);
			InContainer.Empty();
			if (InBudget)
			{
				InBudget->ReleaseBytes(OldBytes - AllocatedBytes(InContainer));
			}
		}
	}

	template<typename ElementType, typename AllocatorType>
	bool RetainArray(TArray<ElementType, AllocatorType>& InArray, const int32 InCapacity, FString& OutError)
	{
		return Private::ReserveArray(FVoxelGenerationAllocationScope::Get(), InArray, InCapacity, OutError);
	}

	template<typename KeyType, typename ValueType, typename KeyFuncs>
	bool RetainMap(TMap<KeyType, ValueType, FDefaultSetAllocator, KeyFuncs>& InMap, const int32 InCapacity, FString& OutError)
	{
		return Private::ReserveSet<TPair<KeyType, ValueType>>(FVoxelGenerationAllocationScope::Get(), InMap, InCapacity, OutError);
	}

	template<typename ElementType, typename KeyFuncs>
	bool RetainSet(TSet<ElementType, KeyFuncs, FDefaultSetAllocator>& InSet, const int32 InCapacity, FString& OutError)
	{
		return Private::ReserveSet<ElementType>(FVoxelGenerationAllocationScope::Get(), InSet, InCapacity, OutError);
	}

	// 嵌套容器必须先逐个释放，或先释放整个正文后用捕获的聚合 bytes 结账。
	template<typename ElementType, typename AllocatorType>
	void FreeRetainedArray(TArray<ElementType, AllocatorType>& InArray)
	{
		Private::FreeContainer(FVoxelGenerationAllocationScope::Get(), InArray);
	}

	template<typename KeyType, typename ValueType, typename KeyFuncs>
	void FreeRetainedMap(TMap<KeyType, ValueType, FDefaultSetAllocator, KeyFuncs>& InMap)
	{
		Private::FreeContainer(FVoxelGenerationAllocationScope::Get(), InMap);
	}

	template<typename ElementType, typename KeyFuncs>
	void FreeRetainedSet(TSet<ElementType, KeyFuncs, FDefaultSetAllocator>& InSet)
	{
		Private::FreeContainer(FVoxelGenerationAllocationScope::Get(), InSet);
	}

	// 两个 default array 的 backing 均已授予原 root；只退 Move 实际释放的旧目标。
	template<typename ElementType>
	bool MoveRetainedArray(TArray<ElementType>& OutArray, TArray<ElementType>& InArray, FString& OutError)
	{
		FVoxelGenerationAllocationScope* Scope = FVoxelGenerationAllocationScope::Get();
		if (!(Scope ? Scope->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
		{
			return false;
		}
		if (&OutArray != &InArray)
		{
			const uint64 OldBytes = AllocatedBytes(OutArray);
			OutArray = MoveTemp(InArray);
			if (Scope)
			{
				Scope->ReleaseBytes(OldBytes);
			}
		}
		return true;
	}
}

/** 必须先于全部受管 scratch 容器声明，使容器先释放 native backing，再退原 Working 租约。 */
class WHFRAMEWORK_API FVoxelGenerationWorkspace
{
public:
	FVoxelGenerationWorkspace();
	// 新 scratch 租约沿原输出上下文，不复制或转移其 backing 所有权。
	explicit FVoxelGenerationWorkspace(const FVoxelGenerationWorkspace* InSource);
	FVoxelGenerationWorkspace(TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, uint64 InRequiredInputBytes = 0, int32 InRequiredLeaseCount = 0,
		uint64 InRequiredWorkingBytes = 0);
	~FVoxelGenerationWorkspace();
	FVoxelGenerationWorkspace(const FVoxelGenerationWorkspace&) = delete;
	FVoxelGenerationWorkspace& operator=(const FVoxelGenerationWorkspace&) = delete;

	bool Check(FString& OutError) const;
	bool IsOwnedBy(const FVoxelTerrainResources& InResources, const FVoxelGenerationConsumerKey& InConsumer) const
	{
		return Resources.Get() == &InResources && Consumer == InConsumer;
	}
	bool Fail(const TCHAR* InError, FString& OutError);
	bool PrepareBytes(uint64 InOldBytes, uint64 InNewBytes, FString& OutError);
	bool SettleBytes(uint64 InOldBytes, uint64 InReservedBytes, uint64 InActualBytes, FString& OutError);
	void ReleaseBytes(uint64 InBytes);
	const FVoxelTerrainResourceLease* GetLease() const { return Lease.Get(); }
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& GetLeaseRef() const { return Lease; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> TakeLease() { return MoveTemp(Lease); }
	uint64 GetRequiredInputBytes() const
	{
		return RequiredInputBytes + (Lease && Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Lease->Bytes() : 0);
	}
	int32 GetRequiredLeaseCount() const { return RequiredLeaseCount + (Lease ? 1 : 0); }
	uint64 GetRequiredWorkingBytes() const
	{
		return RequiredWorkingBytes + (Lease && Lease->GetPool() == EVoxelTerrainResourcePool::Working ? Lease->Bytes() : 0);
	}
	void SetRequirements(uint64 InInputBytes, int32 InLeaseCount, uint64 InWorkingBytes)
	{
		RequiredInputBytes = InInputBytes;
		RequiredLeaseCount = InLeaseCount;
		RequiredWorkingBytes = InWorkingBytes;
	}

	template<typename ElementType, typename AllocatorType>
	bool ReserveArray(TArray<ElementType, AllocatorType>& InArray, const int32 InCapacity, FString& OutError)
	{
		return VoxelGenerationAllocation::Private::ReserveArray(this, InArray, InCapacity, OutError);
	}

	template<typename KeyType, typename ValueType, typename KeyFuncs>
	bool ReserveMap(TMap<KeyType, ValueType, FDefaultSetAllocator, KeyFuncs>& InMap, const int32 InCapacity, FString& OutError)
	{
		return VoxelGenerationAllocation::Private::ReserveSet<TPair<KeyType, ValueType>>(this, InMap, InCapacity, OutError);
	}

	template<typename ElementType, typename KeyFuncs>
	bool ReserveSet(TSet<ElementType, KeyFuncs, FDefaultSetAllocator>& InSet, const int32 InCapacity, FString& OutError)
	{
		return VoxelGenerationAllocation::Private::ReserveSet<ElementType>(this, InSet, InCapacity, OutError);
	}

	template<typename ElementType, typename AllocatorType>
	void FreeArray(TArray<ElementType, AllocatorType>& InArray)
	{
		VoxelGenerationAllocation::Private::FreeContainer(this, InArray);
	}

	template<typename KeyType, typename ValueType, typename KeyFuncs>
	void FreeMap(TMap<KeyType, ValueType, FDefaultSetAllocator, KeyFuncs>& InMap)
	{
		VoxelGenerationAllocation::Private::FreeContainer(this, InMap);
	}

	template<typename ElementType, typename KeyFuncs>
	void FreeSet(TSet<ElementType, KeyFuncs, FDefaultSetAllocator>& InSet)
	{
		VoxelGenerationAllocation::Private::FreeContainer(this, InSet);
	}

private:
	friend class FVoxelGenerationRequiredSourceScope;
	static FVoxelGenerationWorkspace*& ActiveWorkspaceSlot();
	FVoxelGenerationWorkspace* PreviousWorkspace = nullptr;
	FVoxelGenerationAllocationScope* Scope = nullptr;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	uint64 RequiredInputBytes = 0;
	int32 RequiredLeaseCount = 0;
	uint64 RequiredWorkingBytes = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};
