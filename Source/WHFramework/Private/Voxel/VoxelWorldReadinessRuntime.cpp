#include "Voxel/VoxelWorldReadinessRuntime.h"

void FVoxelPlayerClaimState::ClearEvidence()
{
	bSpawnCoreReady = false;
	bResidencyReady = false;
}

bool FVoxelWorldReadinessRuntime::PreparePlayerClaim(FVoxelPlayerClaimState& InOutState,
	const FVoxelPlayerClaimToken& InIdentity, UObject* InPawn, FVoxelPlayerClaimToken& OutToken)
{
	OutToken = {};
	if (!IsValid(InPawn) || !InIdentity.WorldEpoch || !InIdentity.SourceId.IsValid() ||
		!InIdentity.SourceIncarnation || !InIdentity.CandidateRevision || !InIdentity.PawnIncarnation.IsValid()) return false;
	FVoxelPlayerClaimToken Expected = InIdentity;
	Expected.Serial = InOutState.Token.Serial;
	if (IsCurrent(InOutState, Expected, InPawn))
	{
		OutToken = InOutState.Token;
		return true;
	}
	// 活着的另一角色仍持有准备租约；调用者须先明确撤销，不能抢占。
	if (InOutState.Token.IsValid() && InOutState.Pawn.IsValid()) return false;
	if (NextClaimSerial == MAX_uint64) return false;
	InOutState = {};
	InOutState.Token = InIdentity;
	InOutState.Token.Serial = ++NextClaimSerial;
	InOutState.Pawn = InPawn;
	InOutState.bVerificationPending = true;
	OutToken = InOutState.Token;
	return true;
}

bool FVoxelWorldReadinessRuntime::IsCurrent(const FVoxelPlayerClaimState& InState,
	const FVoxelPlayerClaimToken& InToken, const UObject* InPawn) const
{
	return InToken.IsValid() && InState.Token == InToken && IsValid(InPawn) && InState.Pawn.Get() == InPawn;
}

bool FVoxelWorldReadinessRuntime::CommitPlayerClaim(FVoxelPlayerClaimState& InOutState,
	const FVoxelPlayerClaimToken& InToken, UObject* InPawn)
{
	if (!IsCurrent(InOutState, InToken, InPawn) || !InOutState.bSpawnCoreReady ||
		!InOutState.bResidencyReady) return false;
	InOutState.bPlayerOwned = true;
	return true;
}

bool FVoxelWorldReadinessRuntime::AbortPlayerClaim(FVoxelPlayerClaimState& InOutState,
	const FVoxelPlayerClaimToken& InToken)
{
	if (!InToken.IsValid() || InOutState.Token != InToken || InOutState.bPlayerOwned) return false;
	InOutState = {};
	return true;
}
