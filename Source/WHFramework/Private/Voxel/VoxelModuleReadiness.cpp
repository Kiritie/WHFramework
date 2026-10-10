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
	// 出生接管确认的是目标位置；后续移动预测从这里开始，不扫掠出生前的位置。
	Source->LastAcceptedPosition = InToken.CandidateCell;
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
	if (!Interest || Interest->Source.Center != InToken.CandidateCell) co_return;
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
	// 出生只等待交互核心的真实呈现与模拟驻留；整个缓存窗口和远景继续后台准备。
	// 玩家移动仍由 IsTerrainSafeAt 检查目标位置的地形与碰撞，不能提前进入未就绪区域。
	for (const FIntVector& Key : Interest->SimulationSections)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		if (!IsCurrent() || !SimulationSections.Contains(Key)) co_return;
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
	bool bAdvancedClaim = false;
#if !UE_BUILD_SHIPPING
	static double NextClaimDiagnostic = 0.0;
	const bool bLogClaim = FPlatformTime::Seconds() >= NextClaimDiagnostic;
	if (bLogClaim) NextClaimDiagnostic = FPlatformTime::Seconds() + 5.0;
#endif
	for (auto& Pair : Sources)
	{
		if (bAdvancedClaim && FPlatformTime::Seconds() >= Deadline) break;
		FSource& Source = Pair.Value;
#if !UE_BUILD_SHIPPING
		if (bLogClaim && !Source.PlayerClaim.bPlayerOwned)
		{
			int32 MissingData = 0, MissingCollision = 0, MissingFine = 0, MissingSimulation = 0;
			if (const auto* Interest = CurrentInterest->Sources.Find(Pair.Key))
			{
				for (const auto& Key : Interest->DataSections)
				{
					const auto* Section = Runtime->FindSection(Key);
					MissingData += !Section || Section->Status != EVoxelSectionStatus::DataReady;
				}
				for (const auto& Key : Interest->CollisionSections) MissingCollision += !IsCollisionReady(Key);
				for (const auto& Key : Interest->F0CoreSections) MissingFine += !ViewManager || !ViewManager->IsLatestFinePresented(Key);
				for (const auto& Key : Interest->SimulationSections) MissingSimulation += !SimulationSections.Contains(Key);
			}
			UE_LOG(LogTemp, Display, TEXT("Birth claim: token=%d pawn=%d admitted=%d core=%d residency=%d complete=%d contract=%llu/%llu content=%llu/%llu missing=%d/%d/%d/%d"),
				Source.PlayerClaim.Token.IsValid(), Source.PlayerClaim.Pawn.IsValid(), IsSourceAdmitted(Pair.Key),
				Source.PlayerClaim.bSpawnCoreReady, Source.PlayerClaim.bResidencyReady, Source.ClaimVerification.IsComplete(),
				Source.ClaimContractRevision, Source.ContractRevision, Source.ClaimContentRevision, Runtime->GetContentRevision(),
				MissingData, MissingCollision, MissingFine, MissingSimulation);
		}
#endif
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
		// 出生确认不能被持续发布耗尽帧预算而饿死；至少推进一个有界步骤，其余仍遵守期限。
		while (!Source.ClaimVerification.IsComplete() && (!bAdvancedClaim || FPlatformTime::Seconds() < Deadline))
		{
			Source.ClaimVerification.Resume();
			bAdvancedClaim = true;
		}
	}
}
