#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "Voxel/Task/VoxelTaskFairSelection.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Rendering/VoxelTerrainStatistics.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

struct FVoxelSectionCollisionResult;
struct FVoxelSectionMeshResult;
struct FVoxelVoxelProxyData;
struct FVoxelSurfaceTileData;
struct FVoxelWaterSurfaceTileData;
struct FVoxelMacroTileData;
struct FVoxelPersistentSection;
struct FVoxelDetailPlan;
struct FVoxelFineOverlayReadResult;
class FVoxelTerrainFrameContext;

enum class EVoxelWorkClass : uint8
{
	None = 0,
	Critical,
	Warmup,
	ExactData,
	Interactive,
	Visible,
	Boundary,
	Exploration,
	Background,
	Prefetch
};

enum class EVoxelTaskKind : uint8
{
	None = 0,
	GenerateExactBase,
	BuildCollision,
	BuildFineMesh,
	BuildVoxelProxy,
	BuildSurface,
	BuildWater,
	BuildMacro,
	BuildViewTransition,
	BuildVolumeTransition,
	BuildViewCoverage,
	BuildDetails,
	DecodeOverlay,
	EncodeRegion,
	NetworkRepresentation,
	BuildMapTile,

	// 项目层允许复用 Voxel Scheduler 的纯后台任务。
	// 不参与 Voxel 内部 ApplyTask switch。
	ProjectBackground,
	BuildInterest,
	BuildGenerationPlan,
	GenerateVoxelProxy,
	GenerateSurface,
	GenerateMacro,
	BuildNavigation,
	BuildFineShard
};

struct WHFRAMEWORK_API FVoxelTaskStamp
{
	FGuid OwnerId;
	uint64 OwnerIncarnation = 0;
	uint64 WorldEpoch = 0;
	uint64 Token = 0;
	uint64 Revision = 0;
	uint64 GeometryRevision = 0;
	bool bHasGeometryIdentity = false;
	FVoxelTerrainGeometry GeometryIdentity;
	FIntVector Section = FIntVector::ZeroValue;
	FVoxelViewKey ViewKey;
	FVoxelSurfaceTileKey SurfaceKey;
	FVoxelMacroTileKey MacroKey;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;

	bool operator==(const FVoxelTaskStamp& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelTaskStamp& InStamp)
{
	uint32 Hash = GetTypeHash(InStamp.OwnerId);
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.OwnerIncarnation));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.WorldEpoch));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.Token));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.Revision));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.GeometryRevision));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.bHasGeometryIdentity));
	if (InStamp.bHasGeometryIdentity)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(InStamp.GeometryIdentity));
	}
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.Section));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.ViewKey));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.SurfaceKey));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.MacroKey));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.RegistryRevision));
	Hash = HashCombineFast(Hash, GetTypeHash(InStamp.ShapeRevision));
	return Hash;
}

struct WHFRAMEWORK_API FVoxelTaskKey
{
	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	FVoxelTaskStamp Stamp;

	bool operator==(const FVoxelTaskKey& InOther) const
	{
		return Kind == InOther.Kind && Stamp == InOther.Stamp;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelTaskKey& InKey)
{
	return HashCombineFast(
		GetTypeHash(static_cast<uint8>(InKey.Kind)),
		GetTypeHash(InKey.Stamp));
}

/**
 * 项目层异步任务的通用纯 C++ Payload。
 * Payload 不能持有只能在 GameThread 访问的 UObject。
 */
struct WHFRAMEWORK_API FVoxelTaskCustomPayload
{
	mutable TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	virtual ~FVoxelTaskCustomPayload() = default;

	virtual uint64 GetAllocatedBytes() const
	{
		return sizeof(FVoxelTaskCustomPayload);
	}
	virtual uint64 GetResourceAccountedBytes() const
	{
		return 0;
	}
	virtual void LogResourceAccounting(const FVoxelTaskStamp& InStamp) const {}
};

struct WHFRAMEWORK_API FVoxelTaskResult
{
	FVoxelTaskResult() = default;
	FVoxelTaskResult(const FVoxelTaskResult&) = delete;
	FVoxelTaskResult& operator=(const FVoxelTaskResult&) = delete;
	FVoxelTaskResult(FVoxelTaskResult&&) = default;
	FVoxelTaskResult& operator=(FVoxelTaskResult&& InOther);

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputResourceLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputResourceLease;

	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	FVoxelTaskStamp Stamp;

	bool bSuccess = false;
	bool bCanceled = false;
	// 队列容量置换保留准入年龄；生产者可沿原 Stamp 重试，区别于主动撤销。
	bool bAdmissionDisplaced = false;
	bool bKnownEmptyMesh = false;
	bool bContinue = false;
	// Worker 已证明下一片不需要主线程或外部准入；仅在同一有界执行时间片内续跑。
	bool bContinueImmediately = false;
	// 原准备结果的版本接纳状态；容量续跑不重建Worker或尝试身份。
	EVoxelTerrainResourceAdmission PreparedRevisionAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 PreparedRevisionCapacityVersion = 0;
	FString Error;

	// diagnostics
	double QueueMilliseconds = 0.0;
	double ExecuteMilliseconds = 0.0;
	double ApplyMilliseconds = 0.0;
	// 拒收前冻结原结果数字；错误结果释放正文后仍保留可核对的差额。
	uint64 RejectedResultBytes = 0;
	uint64 RejectedAccountedBytes = 0;
	uint64 RejectedPayloadBytes = 0;
	uint64 RejectedPayloadAccountedBytes = 0;
	uint64 RejectedInputLeaseBytes = 0;
	uint64 RejectedOutputLeaseBytes = 0;

	TArray<FVoxelBlockState> BaseBlocks;
	TArray<uint8> Payload;

	TSharedPtr<FVoxelSectionCollisionResult, ESPMode::ThreadSafe> Collision;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> FineMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> VoxelProxyMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> VolumeBoundaryMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> SurfaceMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> WaterMesh;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> MacroMesh;

	TSharedPtr<FVoxelVoxelProxyData, ESPMode::ThreadSafe> VoxelProxy;
	TSharedPtr<FVoxelSurfaceTileData, ESPMode::ThreadSafe> Surface;
	TSharedPtr<FVoxelWaterSurfaceTileData, ESPMode::ThreadSafe> Water;
	TSharedPtr<FVoxelMacroTileData, ESPMode::ThreadSafe> Macro;
	TSharedPtr<FVoxelDetailPlan, ESPMode::ThreadSafe> Details;
	TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe> PersistentSection;
	TSharedPtr<const FVoxelFineOverlayReadResult, ESPMode::ThreadSafe> OverlayReadResult;

	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> CustomPayload;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	void MarkWorkUsed() const { if (WorkReceipt) WorkReceipt->Resolve(true); }

	uint64 ResultBytes() const;
	uint64 GetResourceAccountedBytes() const;
	bool HasHeavyApply() const;
};

struct FVoxelTaskExecutionReservation;

struct WHFRAMEWORK_API FVoxelTaskRequest
{
	FVoxelTaskRequest() = default;
	~FVoxelTaskRequest() = default;
	FVoxelTaskRequest(const FVoxelTaskRequest&) = delete;
	FVoxelTaskRequest& operator=(const FVoxelTaskRequest&) = delete;
	FVoxelTaskRequest(FVoxelTaskRequest&&) = default;
	FVoxelTaskRequest& operator=(FVoxelTaskRequest&& InOther);
#if WITH_DEV_AUTOMATION_TESTS
	// 仅纯选择夹具；不复制回调、租约或回执，返回值不可用于原生接纳。
	FVoxelTaskRequest MakePriorityCopy() const;
#endif

	// 捕获先销毁，再释放对应原账本租约；绑定后不得直接替换函数。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ExecuteBindingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ApplyBindingLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BlockBindingLease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> BindingResources;
	FVoxelGenerationConsumerKey BindingConsumer;
	bool bExecuteBound = false;
	bool bApplyBound = false;
	bool bBlockBound = false;
	// 在排队前冻结实际调用者；续跑沿用完整键，不从缓存控制身份推导。
	FVoxelGenerationConsumerKey GenerationConsumer;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	FVoxelTaskStamp Stamp;
	EVoxelTaskKind Kind = EVoxelTaskKind::None;
	EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
	int32 SourcePriority = 0;
	int32 TerrainStage = INDEX_NONE;
	// 已接纳的局部发布准备/裁剪，持续数据压力下仍须有界完成。
	bool bPublicationContinuation = false;
	bool bTerrainDataOnly = false;

	double DistanceScore = 0.0;
	double ForwardScore = 0.0;
	FGuid SourceStableKey;
	EVoxelTerrainServiceBand ServiceBand = EVoxelTerrainServiceBand::None;
	double FirstRunnableSeconds = -1.0;
	double QueuedAt = 0.0;
	uint64 Sequence = 0;
	EVoxelTaskBlockReason Blocked = EVoxelTaskBlockReason::None;

	uint64 ReservedBytes = 0;
	uint64 InputBytes = 0;

	double AccumulatedExecuteMilliseconds = 0.0;
	bool bResumable = false;
	bool bReceiptCapacityBlocked = false;
	bool bReceiptRegistrationBlocked = false;
	uint64 ReceiptCapacityVersion = 0;
	uint64 ReceiptRequiredInputBytes = 0;
	int32 ReceiptRequiredLeaseCount = 0;
	TFunction<EVoxelTaskBlockReason()> GetBlockReason;
	TFunction<FVoxelTaskResult(const TAtomic<bool>&)> Execute;

	/**
	 * 非空时由任务自己消费结果。
	 * 为空时回到 UVoxelModule::ApplyTask。
	 */
	TFunction<void(FVoxelTaskResult&&)> Apply;

	template<typename FunctorType>
	bool BindExecute(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FunctorType&& InFunction, FString& OutError)
	{
		return BindFunction(InResources, InConsumer, Forward<FunctorType>(InFunction), Execute,
			ExecuteBindingLease, bExecuteBound, ExecuteBindingBytes, ExecuteBindingCount, OutError);
	}

	template<typename FunctorType>
	bool BindApply(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FunctorType&& InFunction, FString& OutError)
	{
		return BindFunction(InResources, InConsumer, Forward<FunctorType>(InFunction), Apply,
			ApplyBindingLease, bApplyBound, ApplyBindingBytes, ApplyBindingCount, OutError);
	}

	template<typename FunctorType>
	bool BindGetBlockReason(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FunctorType&& InFunction, FString& OutError)
	{
		return BindFunction(InResources, InConsumer, Forward<FunctorType>(InFunction), GetBlockReason,
			BlockBindingLease, bBlockBound, BlockBindingBytes, BlockBindingCount, OutError);
	}

	template<typename FactoryType>
	bool WrapExecute(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FactoryType&& InFactory, FString& OutError)
	{
		return WrapFunction(InResources, InConsumer, Forward<FactoryType>(InFactory), Execute,
			ExecuteBindingLease, bExecuteBound, ExecuteBindingBytes, ExecuteBindingCount, OutError);
	}

	template<typename FactoryType>
	bool WrapApply(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FactoryType&& InFactory, FString& OutError)
	{
		return WrapFunction(InResources, InConsumer, Forward<FactoryType>(InFactory), Apply,
			ApplyBindingLease, bApplyBound, ApplyBindingBytes, ApplyBindingCount, OutError);
	}

	uint64 GetBindingBytes() const { return ExecuteBindingBytes + ApplyBindingBytes + BlockBindingBytes; }
	int32 GetBindingLeaseCount() const { return ExecuteBindingCount + ApplyBindingCount + BlockBindingCount; }
	uint64 GetRequiredBindingInputBytes() const { return RequiredBindingInputBytes; }
	int32 GetRequiredBindingInputLeaseCount() const { return RequiredBindingInputLeaseCount; }
	void SetRequiredBindingInput(const uint64 InBytes, const int32 InLeaseCount)
	{
		check(InLeaseCount >= 0);
		RequiredBindingInputBytes = InBytes;
		RequiredBindingInputLeaseCount = InLeaseCount;
	}

private:
	friend class FVoxelTaskScheduler;
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FVoxelSchedulerNativeBackingTestAccess;
	friend class FVoxelTaskAdmissionFailureTest;
#endif
	// 接纳前授予；运行时移出 Request，避免控制对象与自身 Slot 形成引用环。
	TSharedPtr<FVoxelTaskExecutionReservation, ESPMode::ThreadSafe> ExecutionReservation;
	uint64 ExecuteBindingBytes = 0;
	uint64 ApplyBindingBytes = 0;
	uint64 BlockBindingBytes = 0;
	int32 ExecuteBindingCount = 0;
	int32 ApplyBindingCount = 0;
	int32 BlockBindingCount = 0;
	uint64 RequiredBindingInputBytes = 0;
	int32 RequiredBindingInputLeaseCount = 0;

	template<typename SignatureType, typename FunctorType>
	bool BindFunction(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FunctorType&& InFunction,
		TFunction<SignatureType>& OutFunction,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, bool& bOutBound,
		uint64& OutBytes, int32& OutCount, FString& OutError)
	{
		using FFunctor = std::decay_t<FunctorType>;
		static_assert(!std::is_same_v<FFunctor, TFunction<SignatureType>>, "Bind the concrete callable before TFunction allocates");
		if (!InConsumer.IsValid() || (BindingResources &&
			(BindingResources.Get() != &InResources.Get() || BindingConsumer != InConsumer)))
		{
			OutError = TEXT("InvalidConsumer: scheduler callback binding domain mismatch");
			return false;
		}
		const uint64 Bytes = VoxelBoundedAllocation::FunctionBytes<FFunctor, false>();
		if (GetBindingBytes() > MAX_uint64 - RequiredBindingInputBytes ||
			GetBindingLeaseCount() > MAX_int32 - RequiredBindingInputLeaseCount)
		{
			OutError = TEXT("Oversized: scheduler callback required input overflow");
			return false;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
		if (Bytes)
		{
			const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
				Bytes, CandidateLease, 0, GetBindingBytes() + RequiredBindingInputBytes,
				GetBindingLeaseCount() + RequiredBindingInputLeaseCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::InvalidRequest ?
					TEXT("InvalidConsumer: scheduler callback admission request") : Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: scheduler callback binding peak is unreachable") : TEXT("CapacityBlocked: scheduler callback binding input");
				return false;
			}
		}
		TFunction<SignatureType> Candidate(Forward<FunctorType>(InFunction));
		OutFunction = nullptr;
		OutFunction = MoveTemp(Candidate);
		OutLease = MoveTemp(CandidateLease);
		OutBytes = Bytes;
		OutCount = int32(OutLease.IsValid());
		BindingResources = InResources;
		BindingConsumer = InConsumer;
		bOutBound = true;
		OutError.Reset();
		return true;
	}

	// Factory 必须以明确成员生命周期同时持有原函数及原租约，并先销毁函数再退租。
	template<typename SignatureType, typename FactoryType>
	bool WrapFunction(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FactoryType&& InFactory,
		TFunction<SignatureType>& OutFunction,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease, const bool bInBound,
		uint64& OutBytes, int32& OutCount, FString& OutError)
	{
		using FLease = TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>;
		using FFunctor = std::decay_t<decltype(std::declval<FactoryType>()(
			std::declval<TFunction<SignatureType>&&>(), std::declval<FLease>()))>;
		static_assert(!std::is_same_v<FFunctor, TFunction<SignatureType>>, "Wrap with a concrete callable before TFunction allocates");
		if (!bInBound || !OutFunction || !InConsumer.IsValid() ||
			BindingResources.Get() != &InResources.Get() || BindingConsumer != InConsumer)
		{
			OutError = TEXT("InvalidConsumer: scheduler callback wrapper domain mismatch");
			return false;
		}
		const uint64 Bytes = VoxelBoundedAllocation::FunctionBytes<FFunctor, false>();
		if (GetBindingBytes() > MAX_uint64 - RequiredBindingInputBytes ||
			Bytes > MAX_uint64 - GetBindingBytes() - RequiredBindingInputBytes ||
			GetBindingLeaseCount() >= MAX_int32 - RequiredBindingInputLeaseCount)
		{
			OutError = TEXT("Oversized: scheduler callback wrapper peak overflow");
			return false;
		}
		FLease CandidateLease;
		if (Bytes)
		{
			const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
				Bytes, CandidateLease, 0, GetBindingBytes() + RequiredBindingInputBytes,
				GetBindingLeaseCount() + RequiredBindingInputLeaseCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::InvalidRequest ?
					TEXT("InvalidConsumer: scheduler wrapper admission request") : Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: scheduler callback wrapper peak is unreachable") : TEXT("CapacityBlocked: scheduler callback wrapper input");
				return false;
			}
		}
		FFunctor Wrapped = Forward<FactoryType>(InFactory)(MoveTemp(OutFunction), MoveTemp(OutLease));
		TFunction<SignatureType> Candidate(MoveTemp(Wrapped));
		OutFunction = MoveTemp(Candidate);
		OutLease = MoveTemp(CandidateLease);
		OutBytes += Bytes;
		OutCount += int32(OutLease.IsValid());
		OutError.Reset();
		return true;
	}
};

struct WHFRAMEWORK_API FVoxelTaskBudget
{
	int32 MaxConcurrentTasks = 2;
	int32 CriticalReservedTasks = 1;
	int32 MaxPendingTasks = 256;
	uint64 MaxReservedBytes = 128ull * 1024ull * 1024ull;
	uint64 MaxInputBytes = 32ull * 1024ull * 1024ull;
	int32 MaxCompletedResultsPerFrame = 8;
	int32 MaxHeavyCompletedResultsPerFrame = 1;

	int32 MaxFineApplyPerFrame = 8;
	int32 MaxVoxelLODApplyPerFrame = 12;
	int32 MaxSurfaceApplyPerFrame = 16;
	int32 MaxMacroApplyPerFrame = 24;

	int32 MaxConcurrentSurfaceTasks = 1;
	int32 MaxConcurrentMacroTasks = 1;
	int32 MaxConcurrentCoarseTerrainTasks = 1;
	int32 MaxPendingCoarseTerrainTasks = 24;
	int32 MaxWaitingDependencyTasks = 0;
	bool bBorrowIdleKindSlots = false;
};

struct WHFRAMEWORK_API FVoxelTaskKindDiagnostics
{
	uint64 Completed = 0;
	uint64 Failed = 0;
	uint64 Canceled = 0;

	double TotalQueueMilliseconds = 0.0;
	double TotalExecuteMilliseconds = 0.0;
	double TotalApplyMilliseconds = 0.0;

	double MaximumQueueMilliseconds = 0.0;
	double MaximumExecuteMilliseconds = 0.0;
	double MaximumApplyMilliseconds = 0.0;
};

struct WHFRAMEWORK_API FVoxelTaskDiagnostics
{
	static constexpr int32 KindCapacity = static_cast<int32>(EVoxelTaskKind::BuildFineShard) + 1;
	static constexpr int32 BlockCapacity = 11;
	static_assert((1u << (BlockCapacity - 1)) == static_cast<uint32>(EVoxelTaskBlockReason::Oversized));

	template<typename KeyType, typename ValueType, int32 Capacity>
	class TCounts
	{
	public:
		ValueType* Find(const KeyType InKey)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Items[Index].Key == InKey) return &Items[Index].Value;
			}
			return nullptr;
		}

		const ValueType* Find(const KeyType InKey) const
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (Items[Index].Key == InKey) return &Items[Index].Value;
			}
			return nullptr;
		}

		ValueType& FindOrAdd(const KeyType InKey, const ValueType& InDefault = ValueType{})
		{
			if (ValueType* Value = Find(InKey)) return *Value;
			check(Count < Capacity);
			Items[Count].Key = InKey;
			Items[Count].Value = InDefault;
			return Items[Count++].Value;
		}

		ValueType FindRef(const KeyType InKey) const
		{
			const ValueType* Value = Find(InKey);
			return Value ? *Value : ValueType{};
		}

		bool Contains(const KeyType InKey) const { return Find(InKey) != nullptr; }
		int32 Num() const { return Count; }
		TPair<KeyType, ValueType>* begin() { return Items; }
		TPair<KeyType, ValueType>* end() { return Items + Count; }
		const TPair<KeyType, ValueType>* begin() const { return Items; }
		const TPair<KeyType, ValueType>* end() const { return Items + Count; }

		void Reset()
		{
			for (int32 Index = 0; Index < Count; ++Index) Items[Index].Value = ValueType{};
			Count = 0;
		}

	private:
		TPair<KeyType, ValueType> Items[Capacity] = {};
		int32 Count = 0;
	};

	using FKindCounts = TCounts<EVoxelTaskKind, int32, KindCapacity>;
	using FBlockCounts = TCounts<EVoxelTaskBlockReason, int32, BlockCapacity>;
	using FKindBlockCounts = TCounts<EVoxelTaskKind, FBlockCounts, KindCapacity>;
	int32 Pending = 0;
	int32 Running = 0;
	int32 NativeInFlight = 0;
	int32 CompletedAwaitingApply = 0;
	uint64 ContinuationSlicesRetired = 0;
	uint64 BorrowedKindSlots = 0;
	int32 Critical = 0;
	int32 CriticalPending = 0;
	int32 CriticalRunning = 0;
	uint64 CriticalAdmissionDeferrals = 0;
	double LastAdmissionMilliseconds = 0.0;
	double LastResultApplyMilliseconds = 0.0;
	bool bWorkerSelectionFault = false;
	bool bCompletedSelectionFault = false;
	EVoxelTerrainResourceAdmission CompletedSelectionAdmission = EVoxelTerrainResourceAdmission::Granted;

	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;

	TCounts<EVoxelTaskKind, FVoxelTaskKindDiagnostics, KindCapacity> ByKind;
	FKindCounts PendingByKind;
	FKindCounts RunningByKind;
	FKindCounts CompletedAwaitingApplyByKind;
	// 单 flag 可重叠计数，不能求和解释为任务数。
	FKindBlockCounts BlockedByKind;
	int32 BlockedRequests = 0;
	uint64 DuplicateRequests = 0;
	uint64 OversizedRequests = 0;
	uint64 CapacityDeferrals = 0;
	uint64 PendingDisplacements = 0;
	uint64 ResumedPendingDisplacements = 0;
	double DisplacedExecuteMilliseconds = 0.0;
	FKindBlockCounts AdmissionBlockedByKind;
	int32 AdmissionBlockedRequests = 0;
	double OldestAdmissionWaitSeconds = 0.0;
};

class WHFRAMEWORK_API FVoxelTaskScheduler
{
public:
	class FEnqueueBatch
	{
	public:
		explicit FEnqueueBatch(FVoxelTaskScheduler& InScheduler)
			: Scheduler(InScheduler)
		{
			Scheduler.BeginEnqueueBatch();
		}

		~FEnqueueBatch()
		{
			Finish();
		}

		FEnqueueBatch(const FEnqueueBatch&) = delete;
		FEnqueueBatch& operator=(const FEnqueueBatch&) = delete;

		void Finish()
		{
			if (bActive)
			{
				bActive = false;
				Scheduler.EndEnqueueBatch();
			}
		}

	private:
		FVoxelTaskScheduler& Scheduler;
		bool bActive = true;
	};

#if WITH_DEV_AUTOMATION_TESTS
	FVoxelTaskScheduler();
	explicit FVoxelTaskScheduler(bool bInAllowPassive);
	static void* operator new(size_t InSize);
#endif
	~FVoxelTaskScheduler();
	static void operator delete(void* InAddress);
	static bool Create(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InTerrainResources,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InRuntimeResources,
		const FVoxelGenerationConsumerKey& InControlConsumer, TUniquePtr<FVoxelTaskScheduler>& OutScheduler, FString& OutError);

	FVoxelTaskScheduler(const FVoxelTaskScheduler&) = delete;
	FVoxelTaskScheduler& operator=(const FVoxelTaskScheduler&) = delete;

	bool Enqueue(FVoxelTaskRequest&& InRequest);
	void SetPaused(bool bInPaused);
	bool IsPaused() const { return bPaused; }
	bool ObserveDemand(EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp,
		EVoxelTaskBlockReason InReason, uint64 InInputBytes = 0, uint64 InResultBytes = 0);
	bool IsTerminal(const FVoxelTaskKey& InKey) const;
	void UpdatePriorities(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&,
		EVoxelWorkClass&, int32&, double&, double&)> InUpdate);
	void UpdateServicePriorities(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&,
		EVoxelWorkClass&, FVoxelTerrainServicePriority&)> InUpdate);
	void UpdateTerrainStages(TFunctionRef<void(EVoxelTaskKind, const FVoxelTaskStamp&, int32&)> InUpdate);

	void Tick(
		TFunctionRef<void(FVoxelTaskResult&&)> InApply,
		double InMaxApplyMilliseconds = 2.0);

	void CancelSection(const FIntVector& InSection);
	void CancelTask(EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp);
	void CancelMatching(
		TFunctionRef<bool(EVoxelTaskKind, const FVoxelTaskStamp&)> InPredicate);
	void StopAndJoin();

	bool Has(
		const FVoxelTaskStamp& InStamp,
		EVoxelTaskKind InKind) const;

	bool HasSectionTask(const FIntVector& InSection) const;
	void ClearSectionIdleCallback();
#if WITH_DEV_AUTOMATION_TESTS
	void SetSectionIdleCallback(TFunction<void(const FIntVector&)> InCallback);
#endif
	template<typename FunctorType>
	bool SetSectionIdleCallback(FunctorType&& InCallback, FString& OutError)
	{
		using FFunctor = std::decay_t<FunctorType>;
		static_assert(!std::is_same_v<FFunctor, TFunction<void(const FIntVector&)>>, "Bind the concrete idle callback");
		if (!ControlConsumer.IsValid() || !Statistics)
		{
			OutError = TEXT("CapacityBlocked: scheduler control is not initialized");
			return false;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
		const uint64 Bytes = VoxelBoundedAllocation::FunctionBytes<FFunctor, false>();
		if (Bytes)
		{
			const auto Admission = TerrainResources->TryAcquire(ControlConsumer, EVoxelTerrainResourcePool::Input,
				Bytes, CandidateLease, 0, RootInputBytes + BackingBytes + (IdleBindingLease ? IdleBindingLease->Bytes() : 0),
				int32(RootInputBytes != 0) + int32(BackingLease.IsValid()) + int32(IdleBindingLease.IsValid()));
			if (Admission != EVoxelTerrainResourceAdmission::Granted)
			{
				OutError = Admission == EVoxelTerrainResourceAdmission::InvalidRequest ?
					TEXT("InvalidConsumer: scheduler idle callback admission request") : Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: scheduler idle callback peak is unreachable") : TEXT("CapacityBlocked: scheduler idle callback input");
				return false;
			}
		}
		TFunction<void(const FIntVector&)> Candidate(Forward<FunctorType>(InCallback));
		SectionIdleCallback = nullptr;
		SectionIdleCallback = MoveTemp(Candidate);
		IdleBindingLease = MoveTemp(CandidateLease);
		++IdleBindingGeneration;
		OutError.Reset();
		return true;
	}

	int32 ActiveCount() const;
	int32 CriticalCount() const;

	void SetBudget(const FVoxelTaskBudget& InBudget);
	void BindFrameContext(const FVoxelTerrainFrameContext& InContext);
	void SetObservedTerrainStage(int32 InStage);
	int32 GetObservedTerrainStage() const { return ObservedTerrainStage; }

	FVoxelTaskDiagnostics GetDiagnostics() const;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> GetTerrainResources() const;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> GetRuntimeResources() const;
	bool InitializeStatistics(uint64 InWorldEpoch);
	FVoxelGenerationConsumerKey GetControlConsumer() const { return ControlConsumer; }
	EVoxelTerrainResourceAdmission GetLastResourceAdmission() const { return BackingAdmission; }
	TSharedPtr<FVoxelTerrainStatistics, ESPMode::ThreadSafe> GetTerrainStatistics() const { return Statistics; }
	static bool IsHigherPriority(
		const FVoxelTaskRequest& InA,
		const FVoxelTaskRequest& InB);
	static FVoxelTerrainServicePriority ResolveServicePriority(const FVoxelTaskRequest& InRequest);

private:
	friend struct FVoxelTaskExecutionReservation;
	FVoxelTaskScheduler(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InTerrainResources,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InRuntimeResources,
		const FVoxelGenerationConsumerKey& InControlConsumer, uint64 InRootInputBytes);
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FVoxelSchedulerNativeBackingTestAccess;
	friend class FVoxelTaskAdmissionFailureTest;
#endif
	struct FNativeTask : UE::Tasks::FTask
	{
		bool CanRetire() const;
		void Retire();
	};

	struct FSlot
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SlotLease;
		TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
		FVoxelTaskRequest Request;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BudgetErrorLease;
		FString BudgetError;
		TAtomic<bool> Cancel { false };
		FVoxelTaskResult Result;
		double ExecuteMicroseconds = 0.0;
	};

	struct FTaskBody
	{
		TSharedPtr<FSlot, ESPMode::ThreadSafe> Slot;
		FVoxelTaskStamp Stamp;
		void operator()() const;
	};

	struct FRunning
	{
		FVoxelTaskStamp Stamp;
		EVoxelTaskKind Kind = EVoxelTaskKind::None;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
		int32 SourcePriority = 0;
		int32 TerrainStage = INDEX_NONE;
		bool bPublicationContinuation = false;
		bool bTerrainDataOnly = false;

		uint64 Sequence = 0;
		double QueuedAt = 0.0;
		double DistanceScore = 0.0;
		double ForwardScore = 0.0;
		FGuid SourceStableKey;
		EVoxelTerrainServiceBand ServiceBand = EVoxelTerrainServiceBand::None;
		uint64 ServiceChargeSerial = 0;
		bool bServiceNear = false;
		bool bServiceSettled = false;
		uint64 ReservedBytes = 0;
		uint64 BudgetErrorBytes = 0;

		TSharedPtr<FVoxelTaskExecutionReservation, ESPMode::ThreadSafe> ExecutionReservation;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> SlotLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NativeLease;
		TSharedPtr<FSlot, ESPMode::ThreadSafe> Slot;
		FNativeTask Task;
	};

	struct FCompleted
	{
		FVoxelTaskResult Result;
		EVoxelWorkClass WorkClass = EVoxelWorkClass::None;
		int32 TerrainStage = INDEX_NONE;
		FVoxelTaskRequest Request;
	};

	static bool UsesSectionKey(EVoxelTaskKind InKind);
	int32 RunningCount(EVoxelTaskKind InKind) const;
	bool CanStartKind(EVoxelTaskKind InKind) const;

	void RetireCompletedTasks();
	void Pump();
	void BeginEnqueueBatch();
	void EndEnqueueBatch();
	void QueueCanceled(FVoxelTaskRequest&& InRequest, EVoxelTaskBlockReason InFailure = EVoxelTaskBlockReason::None, bool bInAdmissionDisplaced = false);
	void CancelAttempt(const FVoxelTaskKey& InKey, uint64 InSequence);

	void AddActive(
		const FVoxelTaskStamp& InStamp,
		EVoxelTaskKind InKind,
		EVoxelWorkClass InWorkClass);

	void RemoveActive(
		const FVoxelTaskStamp& InStamp,
		EVoxelTaskKind InKind,
		EVoxelWorkClass InWorkClass,
		bool bForgetAdmission = true);

	void RecordCompletedResult(const FVoxelTaskResult& InResult);
	bool PrepareBacking(uint64 InNewBytes);
	void SettleBacking(uint64 InOldBytes, uint64 InReservedBytes, uint64 InActualBytes);
	bool PrepareAdmission(const FVoxelTaskKey& InKey);
	struct FAdmissionAttempt;
	FAdmissionAttempt& FindOrAddAdmission(const FVoxelTaskKey& InKey);
	void UnlinkAdmission(const FVoxelTaskKey& InKey, const FAdmissionAttempt& InAttempt);
	void RemoveAdmission(const FVoxelTaskKey& InKey);
	void ClearAdmissions();
	bool PrepareQueues(int32 InAdditionalTasks);
	void FreeBacking();
	void FlushSectionIdleNotifications();
	void RefreshBlockReasons();
	void LogAdmissionStall(double InNow);
	void VisitPriorityStates(TFunctionRef<void(FVoxelTaskRequest&)> InUpdate);
	FVoxelGenerationConsumerKey ResolveNativeConsumer(const FVoxelTaskRequest& InRequest) const;
	EVoxelTerrainResourceAdmission ReserveNativeExecution(FVoxelTaskRequest& InRequest,
		EVoxelTaskBlockReason& OutBlocked);

	template<typename ElementType, typename AllocatorType>
	bool ReserveArray(TArray<ElementType, AllocatorType>& InArray, const int32 InCount)
	{
		if (InCount <= InArray.Max()) return true;
		const uint64 OldBytes = VoxelGenerationAllocation::AllocatedBytes(InArray);
		const uint64 NewBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<ElementType, AllocatorType>(InCount);
		if (!PrepareBacking(NewBytes)) return false;
		InArray.Reserve(InCount);
		SettleBacking(OldBytes, NewBytes, VoxelGenerationAllocation::AllocatedBytes(InArray));
		return true;
	}

	template<typename ElementType, typename ContainerType>
	bool ReserveSet(ContainerType& InContainer, const int32 InCount)
	{
		if (VoxelGenerationAllocation::Private::SetCanReserve<ElementType>(InContainer, InCount)) return true;
		const uint64 OldBytes = VoxelGenerationAllocation::Private::SetAllocatedBytes<ElementType>(InContainer);
		const uint64 NewBytes = VoxelGenerationAllocation::Private::SetReserveBytes<ElementType>(InContainer, InCount);
		if (!PrepareBacking(NewBytes)) return false;
		InContainer.Reserve(InCount);
		SettleBacking(OldBytes, NewBytes, VoxelGenerationAllocation::Private::SetAllocatedBytes<ElementType>(InContainer));
		return true;
	}

private:
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BackingLease;
	FVoxelGenerationConsumerKey ControlConsumer;
	EVoxelTerrainResourceAdmission BackingAdmission = EVoxelTerrainResourceAdmission::None;
	uint64 BackingBytes = 0;
	uint64 RootInputBytes = 0;
	uint64 RequiredAdmissionInputBytes = 0;
	int32 RequiredAdmissionLeaseCount = 0;
	TArray<FVoxelTaskRequest> Pending;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> TerrainResources;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> RuntimeResources;
	TArray<FRunning> Running;
	TArray<FRunning> AwaitingApply;
	TArray<FCompleted> Canceled;
	TArray<FVoxelTerrainServiceCandidate> WorkerCandidates;
	TArray<FVoxelTerrainServiceCandidate> CompletedCandidates;
	TArray<int32> DisplacedIndices;
	TArray<FIntVector> SectionIdleNotifications;

	TSet<FVoxelTaskKey> ActiveKeys;
	TMap<FIntVector, int32> SectionTaskCounts;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> IdleBindingLease;
	uint64 IdleBindingGeneration = 0;
	TFunction<void(const FIntVector&)> SectionIdleCallback;

	FVoxelTaskBudget Budget;
	const FVoxelTerrainFrameContext* FrameContext = nullptr;
	FVoxelTaskDiagnostics Diagnostics;
	TSharedPtr<FVoxelTerrainStatistics, ESPMode::ThreadSafe> Statistics;
	FGuid StatisticsOwnerId = FGuid::NewGuid();
	uint64 StatisticsWorldEpoch = 0;
	uint64 StatisticsRootInputBytes = 0;
	struct FAdmissionAttempt
	{
		FVoxelTaskKey Previous;
		FVoxelTaskKey Next;
		double FirstRequested = 0.0;
		uint64 Sequence = 0;
		EVoxelTaskBlockReason Blocked = EVoxelTaskBlockReason::None;
		uint64 InputBytes = 0;
		uint64 ResultBytes = 0;
		uint64 Rejections = 0;
	};
	struct FCancellationCursor
	{
		FCancellationCursor* Previous = nullptr;
		FVoxelTaskKey Next;
	};
	TMap<FVoxelTaskKey, FAdmissionAttempt> AdmissionAttempts;
	FVoxelTaskKey AdmissionHead;
	FVoxelTaskKey AdmissionTail;
	FCancellationCursor* CancellationCursor = nullptr;
	FVoxelTerrainWorkSelection WorkerSelection;
	FVoxelTerrainWorkSelection CompletedSelection;
	FVoxelTaskDiagnostics::TCounts<EVoxelTaskKind, double, FVoxelTaskDiagnostics::KindCapacity> WorkerCostEstimates;
	FVoxelTaskDiagnostics::TCounts<EVoxelTaskKind, double, FVoxelTaskDiagnostics::KindCapacity> ApplyCostEstimates;
	uint64 NextSequence = 0;
	uint64 BlockRefreshSequence = 0;

	double LastBudgetWarning = -10.0;
	double AdmissionStallStarted = -1.0;
	double LastAdmissionStallLog = -15.0;
	bool bStopped = false;
	bool bPaused = false;
	bool bAllowPassive = false;
	bool bPumping = false;
	int32 EnqueueBatchDepth = 0;
	bool bBatchPumpRequested = false;
	bool bTicking = false;
	bool bFlushingIdle = false;
	int32 ObservedTerrainStage = 3;
	uint64 ReservedBytes = 0;
	uint64 QueuedInputBytes = 0;
	uint64 RunningContinuationInputBytes = 0;
	int32 CriticalTaskCount = 0;
	uint64 CriticalAdmissionDeferrals = 0;
};
