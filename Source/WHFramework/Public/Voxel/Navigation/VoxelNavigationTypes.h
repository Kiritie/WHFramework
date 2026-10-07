#pragma once

#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Task/VoxelTerrainResources.h"

struct FVoxelRegistrySnapshot;
class FVoxelShapeRegistry;
struct FVoxelPhysicsReceipt;
class FVoxelTerrainWorkReceipt;
struct FVoxelNavigationInvalidationSource;

/** 表面/tile 的实际使用者；生产撤销在 GT 发出，搜索阶段只记录撤销意图。 */
class WHFRAMEWORK_API IVoxelNavigationInvalidationConsumer
{
public:
	virtual ~IVoxelNavigationInvalidationConsumer();
	virtual void OnNavigationInputRevoked() = 0;
};

/** 调用者预分配的反向边。地址在订阅期间固定，不在撤销时分配监听容器。 */
class WHFRAMEWORK_API FVoxelNavigationInvalidationObserver
{
public:
	FVoxelNavigationInvalidationObserver();
	~FVoxelNavigationInvalidationObserver();
	FVoxelNavigationInvalidationObserver(
		FVoxelNavigationInvalidationObserver&& InOther);
	FVoxelNavigationInvalidationObserver(
		const FVoxelNavigationInvalidationObserver&) = delete;
	FVoxelNavigationInvalidationObserver& operator=(
		const FVoxelNavigationInvalidationObserver&) = delete;
	bool Attach(
		FVoxelNavigationInvalidationSource& InSource,
		TWeakPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> InConsumer);
	void Detach();

private:
	friend struct FVoxelNavigationInvalidationSource;
	TAtomic<FVoxelNavigationInvalidationSource*> Source{nullptr};
	FVoxelNavigationInvalidationObserver* Previous = nullptr;
	FVoxelNavigationInvalidationObserver* Next = nullptr;
	TWeakPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> Consumer;
};

/** 单次发布的撤销信号，同时服务冻结表面和实际 Native tile 代。 */
struct WHFRAMEWORK_API FVoxelNavigationInvalidationSource
{
	void Publish();
	void Revoke();
	bool IsCurrent() const;

private:
	friend class FVoxelNavigationInvalidationObserver;
	void RemoveLocked(
		FVoxelNavigationInvalidationObserver& InObserver);
	FCriticalSection ObserverLock;
	FVoxelNavigationInvalidationObserver* First = nullptr;
	// 0=待发布，1=当前，2=已撤销；同一次发布不能复活。
	TAtomic<uint8> State{0};
};

/** Section 的实际导航消费者。工作线程释放只改变计数，由既有 GT 退役队列处理。 */
struct WHFRAMEWORK_API FVoxelNavigationSectionConsumers
{
	bool TryAcquire();
	void Release();
	void SetAdmissionOpen(
		bool bInOpen);
	void CloseWorld();
	bool IsWorldOpen() const;
	int32 Num() const;

private:
	friend struct FVoxelNavigationSurfaceAccess;
	bool TryAcquireContinuation();
	TAtomic<int32> Readers{0};
	TAtomic<bool> bAdmissionOpen{true};
	TAtomic<bool> bWorldOpen{true};
};

/** 单次表面发布的读取令牌；旧 tile 不得借用新版表面的可用状态。 */
struct WHFRAMEWORK_API FVoxelNavigationSurfaceAccess
{
	explicit FVoxelNavigationSurfaceAccess(
		TSharedRef<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe> InConsumers);
	bool TryAcquire();
	void Release();
	void Publish();
	void Revoke();
	bool IsCurrent() const;
	bool Observe(
		FVoxelNavigationInvalidationObserver& InObserver,
		TWeakPtr<IVoxelNavigationInvalidationConsumer, ESPMode::ThreadSafe> InConsumer);

private:
	friend class FVoxelNavigationSurfaceReadLease;
	bool TryAcquireContinuation(
		const FVoxelNavigationSurfaceAccess& InPrevious);
	TSharedRef<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe> Consumers;
	FVoxelNavigationInvalidationSource Invalidation;
};

struct WHFRAMEWORK_API FVoxelNavigationInputVersion
{
	uint64 WorldEpoch = 0;
	FIntVector Section = FIntVector::ZeroValue;
	uint64 SectionIncarnation = 0;
	uint64 SectionToken = 0;
	uint64 SectionRevision = 0;
	uint64 LocalGeometryRevision = 0;
	uint64 AboveIncarnation = 0;
	uint64 AboveToken = 0;
	uint64 AboveRevision = 0;
	uint64 AboveGeometryRevision = 0;
	uint64 SecondAboveIncarnation = 0;
	uint64 SecondAboveToken = 0;
	uint64 SecondAboveRevision = 0;
	uint64 SecondAboveGeometryRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;
	FGuid PhysicsSceneEpoch;
	FGuid PhysicsOwnerId;
	uint64 CollisionBodyGeneration = 0;
	double CellSize = 0.0;
	double Clearance = 0.0;
	bool MatchesPhysicsReceipt(const FVoxelPhysicsReceipt& InReceipt) const;
	bool operator==(const FVoxelNavigationInputVersion& InOther) const = default;
};

struct WHFRAMEWORK_API FVoxelNavigationSurface
{
	FVoxelNavigationInputVersion InputVersion;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	FBox Bounds = FBox(ForceInit);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	mutable TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	TSharedPtr<FVoxelNavigationSurfaceAccess, ESPMode::ThreadSafe> Access;
};

/** 移动所有权的真实表面读取引用，同时保留冻结几何和 Section 的 Nav/Data/Physics 需求。 */
class WHFRAMEWORK_API FVoxelNavigationSurfaceReadLease
{
public:
	FVoxelNavigationSurfaceReadLease();
	~FVoxelNavigationSurfaceReadLease();
	FVoxelNavigationSurfaceReadLease(
		FVoxelNavigationSurfaceReadLease&& InOther);
	FVoxelNavigationSurfaceReadLease& operator=(
		FVoxelNavigationSurfaceReadLease&& InOther);
	FVoxelNavigationSurfaceReadLease(
		const FVoxelNavigationSurfaceReadLease&) = delete;
	FVoxelNavigationSurfaceReadLease& operator=(
		const FVoxelNavigationSurfaceReadLease&) = delete;
	bool Acquire(
		TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface);
	bool AcquireContinuation(
		TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> InSurface,
		const FVoxelNavigationSurfaceReadLease& InPrevious);
	bool IsCurrent() const;
	const FVoxelNavigationSurface* Get() const;
	void Reset();

private:
	TSharedPtr<const FVoxelNavigationSurface, ESPMode::ThreadSafe> Surface;
};

class WHFRAMEWORK_API FVoxelNavigationSurfaceBuilder
{
public:
	static bool Build(const FVoxelSectionSnapshot& InSection, const FVoxelSectionSnapshot* InAbove,
		const FVoxelRegistrySnapshot& InRegistry, const FVoxelShapeRegistry& InShapes,
		double InCellSize, double InClearance, FVoxelNavigationSurface& OutSurface, const TAtomic<bool>* InCancel = nullptr,
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources = nullptr,
		const FVoxelGenerationConsumerKey& InConsumer = {}, FString* OutError = nullptr,
		const FVoxelSectionSnapshot* InSecondAbove = nullptr);
};
