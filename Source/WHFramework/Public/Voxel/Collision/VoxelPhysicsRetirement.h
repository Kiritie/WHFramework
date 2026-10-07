#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

class FPhysScene_Chaos;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
class FVoxelTerrainFrameContext;
class FVoxelTerrainWorkReceipt;
struct FVoxelPhysicsRetirementNode;

namespace Chaos
{
	struct FPhysicsObject;
	class FSingleParticlePhysicsProxy;
	class FPBDRigidsSolver;
}

struct WHFRAMEWORK_API FVoxelPhysicsRetirementPayload
{
	virtual ~FVoxelPhysicsRetirementPayload() = default;
};

// ordinal 属于同一 Callback 的执行顺序，不使用可能因回滚倒退的 PhysicsStep。
struct WHFRAMEWORK_API FVoxelNativeRetirementFence
{
	uint64 DestructionOrdinal = 0;
	bool bObservedDestruction = false;

	bool ObserveDestruction(uint64 InOrdinal);
	bool IsPastDestruction(uint64 InOrdinal) const;
};

struct WHFRAMEWORK_API FVoxelPhysicsRetirementTicket
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelPhysicsRetirementPayload, ESPMode::ThreadSafe> Payload;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> CookWorkReceipt;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> InputWorkReceipt;
	TSharedPtr<FVoxelPhysicsRetirementNode, ESPMode::ThreadSafe> PreparedNodes[17];
	FVoxelGenerationConsumerKey Consumer;
	uint64 PhysicsObjectKey = 0;
	Chaos::FSingleParticlePhysicsProxy* Actor = nullptr;
	FVoxelNativeRetirementFence Fence;
	TAtomic<bool> bNativeRetired{false};
	TAtomic<bool> bPhysicsCommitted{false};
	bool bRegistered = false;
	bool bInstalled = false;
	bool bRemovalIssued = false;
	FVoxelPhysicsRetirementTicket* ActivePrevious = nullptr;
	FVoxelPhysicsRetirementTicket* ActiveNext = nullptr;
	FVoxelPhysicsRetirementTicket* PendingNext = nullptr;
	FVoxelPhysicsRetirementTicket* ReadyNext = nullptr;
	bool ConfirmPhysicsCommitted(Chaos::FPBDRigidsSolver& InSolver);
};

// 每个地形世界仅一个 Chaos watcher；只访问事件涉及的固定键路径和退休票据。
class WHFRAMEWORK_API FVoxelPhysicsRetirement final
{
public:
	FVoxelPhysicsRetirement();
	static TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> Create(
		FPhysScene_Chaos& InScene,
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	~FVoxelPhysicsRetirement();
	bool PrepareTicket(Chaos::FSingleParticlePhysicsProxy* InActor,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelPhysicsRetirementPayload, ESPMode::ThreadSafe> InPayload,
		TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe>& InOutTicket,
		bool& bOutCapacityBlocked);
	void CancelDetached(const TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe>& InTicket);
	void Close();
	void Advance(FVoxelTerrainFrameContext& InFrame);

private:
	struct FState;
	struct FCallback;
	struct FTickerTask;
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
};
