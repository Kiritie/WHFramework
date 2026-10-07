#include "Voxel/VoxelModule.h"

#include "GameFramework/Actor.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"

bool UVoxelModule::IsSourceOwnedBy(const FGuid& InSource, const UObject* InOwner) const
{
	const FSource* Source = Sources.Find(InSource);
	return Source && InOwner && Source->Owner.Get() == InOwner;
}

bool UVoxelModule::PreparePlayerClaim(const FGuid& InSource, UObject* InPawn,
	const FGuid& InPawnIncarnation, const FIntVector& InCandidateCell, FVoxelPlayerClaimToken& OutToken)
{
	check(IsInGameThread());
	FSource* Source = Sources.Find(InSource);
	const AActor* Actor = Cast<AActor>(InPawn);
	OutToken = {};
	if (!IsReady() || !Source || !Source->Owner.IsValid() || !Actor || Actor->GetWorld() != GetWorld() ||
		Source->Value.Purpose != EVoxelStreamingSourcePurpose::InitialSpawn || Source->Value.Center != InCandidateCell) return false;
	FVoxelPlayerClaimToken Identity;
	Identity.WorldEpoch = Epoch;
	Identity.SourceId = InSource;
	Identity.SourceIncarnation = Source->Value.RegistrationOrder;
	Identity.CandidateRevision = Source->CandidateRevision;
	Identity.ControlRevision = Source->ControlRevision;
	Identity.PawnIncarnation = InPawnIncarnation;
	Identity.CandidateCell = InCandidateCell;
	return ReadinessRuntime.PreparePlayerClaim(Source->PlayerClaim, Identity, InPawn, OutToken);
}

bool UVoxelModule::CommitPlayerClaim(const FVoxelPlayerClaimToken& InToken, UObject* InPawn)
{
	check(IsInGameThread());
	FSource* Source = Sources.Find(InToken.SourceId);
	const AActor* Actor = Cast<AActor>(InPawn);
	FIntVector Cell;
	if (!IsReady() || !Source || !Actor || InToken.WorldEpoch != Epoch ||
		InToken.SourceIncarnation != Source->Value.RegistrationOrder ||
		InToken.CandidateRevision != Source->CandidateRevision ||
		!VoxelCoord::FromWorld(Actor->GetActorLocation(), BlockSize(), Cell) || Cell != InToken.CandidateCell) return false;
	if (Source->PlayerClaim.bPlayerOwned) return IsPlayerClaimCommitted(InToken, InPawn);
	if (Source->ControlRevision != InToken.ControlRevision || Source->ClaimContractRevision != Source->ContractRevision ||
		Source->ClaimContentRevision != Runtime->GetContentRevision() || !IsSourceAdmitted(InToken.SourceId) ||
		!ReadinessRuntime.CommitPlayerClaim(Source->PlayerClaim, InToken, InPawn)) return false;
	check(Source->ControlRevision < MAX_uint64);
	Source->Owner = InPawn;
	Source->Value.Purpose = EVoxelStreamingSourcePurpose::Observer;
	++Source->ControlRevision;
	Source->ClaimVerification.Reset();
	// 只转移原记录的控制权。原窗口、Anchor 和所有驻留计数不经过零。
	check(SourceRevision < MAX_uint64);
	++SourceRevision;
	bInterestDirty = true;
	return true;
}

void UVoxelModule::AbortPlayerClaim(const FVoxelPlayerClaimToken& InToken)
{
	check(IsInGameThread());
	if (FSource* Source = Sources.Find(InToken.SourceId); Source &&
		ReadinessRuntime.AbortPlayerClaim(Source->PlayerClaim, InToken))
	{
		Source->ClaimVerification.Reset();
		Source->ClaimContractRevision = 0;
	}
}

bool UVoxelModule::IsPlayerClaimCommitted(const FVoxelPlayerClaimToken& InToken, const UObject* InPawn) const
{
	const FSource* Source = Sources.Find(InToken.SourceId);
	return IsReady() && Source && InToken.WorldEpoch == Epoch &&
		InToken.SourceIncarnation == Source->Value.RegistrationOrder &&
		Source->PlayerClaim.bPlayerOwned && ReadinessRuntime.IsCurrent(Source->PlayerClaim, InToken, InPawn) &&
		Source->Owner.Get() == InPawn;
}

FVoxelGameThreadContinuation UVoxelModule::VerifyPlayerClaimSteps(FVoxelPlayerClaimToken InToken,
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> InInterest)
{
	ON_SCOPE_EXIT
	{
		// 固定需求根可能跨过多次全局提交；在后台释放最后一个旧根读者。
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
			[Pinned = MoveTemp(InInterest)]() mutable { Pinned.Reset(); });
	};
	const FVoxelSourceInterest* Interest = InInterest->Sources.Find(InToken.SourceId);
	if (!Interest || Interest->Source.Center != InToken.CandidateCell || Interest->bRequiresTravelBarrier) co_return;
	const uint64 ContractRevision = Sources.FindChecked(InToken.SourceId).ContractRevision;
	auto IsCurrent = [this, InToken, ContractRevision]()
	{
		const FSource* Source = Sources.Find(InToken.SourceId);
		return Source && Source->PlayerClaim.Token == InToken && Source->PlayerClaim.Pawn.IsValid() &&
			InToken.WorldEpoch == Epoch && InToken.SourceIncarnation == Source->Value.RegistrationOrder &&
			InToken.CandidateRevision == Source->CandidateRevision && Source->ContractRevision == ContractRevision &&
			Source->ClaimContentRevision == Runtime->GetContentRevision();
	};
	const bool bVisual = Interest->Source.Has(EVoxelStreamingCapability::FineVisual);
	for (const FIntVector& Key : Interest->DataSections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!IsCurrent()) co_return;
		const FVoxelSection* Section = Runtime->FindSection(Key);
		if (!Section || Section->Status != EVoxelSectionStatus::DataReady) co_return;
	}
	for (const FIntVector& Key : Interest->CollisionSections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!IsCurrent() || !IsCollisionReady(Key)) co_return;
	}
	for (const FIntVector& Key : Interest->F0CoreSections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!IsCurrent() || (bVisual && (!ViewManager || !ViewManager->IsLatestFinePresented(Key)))) co_return;
	}
	if (!IsCurrent()) co_return;
	Sources.FindChecked(InToken.SourceId).PlayerClaim.bSpawnCoreReady = true;
	if (bVisual)
	{
		for (const FVoxelFineShardKey& Shard : Interest->GetCurrentWindow().Shards)
		{
			const int32 Side = Shard.SideCells / 16;
			const FIntVector First = Shard.Min / 16;
			for (int32 Z = 0; Z < Side; ++Z)
				for (int32 Y = 0; Y < Side; ++Y)
					for (int32 X = 0; X < Side; ++X)
					{
						co_await FVoxelGameThreadContinuation::Yield();
						if (!IsCurrent() || !ViewManager->IsLatestFinePresented(First + FIntVector(X, Y, Z))) co_return;
					}
		}
	}
	for (const FIntVector& Key : Interest->SimulationSections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!IsCurrent() || !SimulationSections.Contains(Key)) co_return;
	}
	if (Interest->Source.Has(EVoxelStreamingCapability::WorldVisual))
	{
		for (const auto& Pair : InInterest->Geometry)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			if (!IsCurrent()) co_return;
			const auto& Geometry = Pair.Value;
			if (Geometry.Key.Representation == EVoxelTerrainRepresentation::Fine) continue;
			FIntVector Min, Max;
			if (!Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Min, Max)) co_return;
			const int32 Radius = Geometry.Key.Representation == EVoxelTerrainRepresentation::VoxelProxy ?
				Interest->Source.View.VoxelProxyRadiusCells : Geometry.Key.Representation == EVoxelTerrainRepresentation::Surface ?
				Interest->Source.View.SurfaceRadiusCells : Interest->Source.View.MacroRadiusCells;
			const int64 DX = FMath::Max<int64>(FMath::Max<int64>(int64(Min.X) - InToken.CandidateCell.X,
				int64(InToken.CandidateCell.X) - Max.X), 0);
			const int64 DY = FMath::Max<int64>(FMath::Max<int64>(int64(Min.Y) - InToken.CandidateCell.Y,
				int64(InToken.CandidateCell.Y) - Max.Y), 0);
			if (DX > Radius || DY > Radius) continue;
			if (!ViewManager->IsTerrainGeometryPresented(Geometry)) co_return;
		}
	}
	if (IsCurrent()) Sources.FindChecked(InToken.SourceId).PlayerClaim.bResidencyReady = true;
}

void UVoxelModule::AdvancePlayerClaims()
{
	const double Deadline = TerrainFrameContext.Deadline(0.05);
	const auto Render = ViewManager ? ViewManager->CapturePresentedSnapshot() : nullptr;
	const auto Physics = CollisionPresenter ? CollisionPresenter->CaptureSnapshot() : nullptr;
	const uint64 RenderSerial = Render ? Render->GetSerial() : 0;
	const uint64 PhysicsSerial = Physics ? Physics->Serial : 0;
	for (auto& Pair : Sources)
	{
		if (FPlatformTime::Seconds() >= Deadline) break;
		FSource& Source = Pair.Value;
		if (!Source.PlayerClaim.Token.IsValid() || Source.PlayerClaim.bPlayerOwned) continue;
		if (!Source.PlayerClaim.Pawn.IsValid())
		{
			ReadinessRuntime.AbortPlayerClaim(Source.PlayerClaim, Source.PlayerClaim.Token);
			Source.ClaimVerification.Reset();
			continue;
		}
		if (!IsSourceAdmitted(Pair.Key)) continue;
		const bool bInputsChanged = Source.ClaimContractRevision != Source.ContractRevision ||
			Source.ClaimContentRevision != Runtime->GetContentRevision();
		const bool bCoverageChanged = Source.ClaimRenderSerial != RenderSerial || Source.ClaimPhysicsSerial != PhysicsSerial ||
			Source.ClaimSimulationRevision != SimulationResidencyRevision;
		if (bInputsChanged || (!Source.PlayerClaim.bResidencyReady && Source.ClaimVerification.IsComplete() && bCoverageChanged))
		{
			Source.PlayerClaim.ClearEvidence();
			Source.ClaimContractRevision = Source.ContractRevision;
			Source.ClaimContentRevision = Runtime->GetContentRevision();
			Source.ClaimRenderSerial = RenderSerial;
			Source.ClaimPhysicsSerial = PhysicsSerial;
			Source.ClaimSimulationRevision = SimulationResidencyRevision;
			Source.ClaimVerification = VerifyPlayerClaimSteps(Source.PlayerClaim.Token, CurrentInterest);
		}
		// 逐个必要项核对；远处任务未清空不会成为出生条件。
		while (!Source.ClaimVerification.IsComplete() && FPlatformTime::Seconds() < Deadline)
			Source.ClaimVerification.Resume();
	}
}
