#pragma once

#include "CoreMinimal.h"

// 令牌绑定一次角色生命期和一个最终候选，不能以相同坐标代替身份校验。
struct WHFRAMEWORK_API FVoxelPlayerClaimToken
{
	uint64 WorldEpoch = 0;
	FGuid SourceId;
	uint64 SourceIncarnation = 0;
	uint64 CandidateRevision = 0;
	uint64 ControlRevision = 0;
	FGuid PawnIncarnation;
	FIntVector CandidateCell = FIntVector::ZeroValue;
	uint64 Serial = 0;

	bool operator==(const FVoxelPlayerClaimToken& InOther) const = default;
	bool IsValid() const { return WorldEpoch && SourceId.IsValid() && SourceIncarnation &&
		CandidateRevision && PawnIncarnation.IsValid() && Serial; }
};

struct WHFRAMEWORK_API FVoxelPlayerClaimState
{
	FVoxelPlayerClaimToken Token;
	TWeakObjectPtr<UObject> Pawn;
	bool bSpawnCoreReady = false;
	bool bResidencyReady = false;
	bool bPlayerOwned = false;

	void ClearEvidence();
};

// Module 唯一持有运行时；状态嵌在原 Source 记录中，不另建第二套需求或 Window。
class WHFRAMEWORK_API FVoxelWorldReadinessRuntime
{
public:
	bool PreparePlayerClaim(FVoxelPlayerClaimState& InOutState,
		const FVoxelPlayerClaimToken& InIdentity, UObject* InPawn,
		FVoxelPlayerClaimToken& OutToken);
	bool CommitPlayerClaim(FVoxelPlayerClaimState& InOutState,
		const FVoxelPlayerClaimToken& InToken, UObject* InPawn);
	bool AbortPlayerClaim(FVoxelPlayerClaimState& InOutState,
		const FVoxelPlayerClaimToken& InToken);
	bool IsCurrent(const FVoxelPlayerClaimState& InState,
		const FVoxelPlayerClaimToken& InToken, const UObject* InPawn) const;

private:
	uint64 NextClaimSerial = 0;
};
