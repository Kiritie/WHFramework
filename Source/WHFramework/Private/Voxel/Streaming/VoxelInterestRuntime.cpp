#include "Voxel/Streaming/VoxelInterestRuntime.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Misc/ScopeExit.h"

namespace
{
	constexpr int32 RuntimeInterestSectionSide = 16;
	template <typename KeyType, typename ValueType> bool SameRuntimeMap(const TMap<KeyType, ValueType>& InA, const TMap<KeyType, ValueType>& InB)
	{
		if (InA.Num() != InB.Num())
		{
			return false;
		}

		for (const auto& Pair : InA)
		{
			const ValueType* Other = InB.Find(Pair.Key);
			if (!Other || *Other != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	void MergeDemand(FVoxelExactDemand& InOutDemand, const FVoxelExactDemand& InDemand,
		const FVoxelStreamingSource& InSource, const FIntVector& InKey)
	{
		InOutDemand.bData |= InDemand.bData;
		InOutDemand.bExact |= InDemand.bExact;
		InOutDemand.bCollision |= InDemand.bCollision;
		InOutDemand.bSimulation |= InDemand.bSimulation;
		InOutDemand.bFineRender |= InDemand.bFineRender;
		InOutDemand.bFineData |= InDemand.bFineData;
		InOutDemand.bWarmupData |= InDemand.bWarmupData;
		InOutDemand.bWarmupCollision |= InDemand.bWarmupCollision;
		InOutDemand.bMovementCriticalFine |= InDemand.bMovementCriticalFine;
		InOutDemand.bMovementCriticalCollision |= InDemand.bMovementCriticalCollision;
		InOutDemand.bMovementCriticalData |= InDemand.bMovementCriticalData;
		InOutDemand.Priority = FMath::Min(InOutDemand.Priority, InSource.GetSchedulingPriority());
		const FVector Delta = FVector(InKey * RuntimeInterestSectionSide + FIntVector(RuntimeInterestSectionSide / 2) - InSource.Center);
		const double Distance = Delta.Size();
		InOutDemand.DistanceCells = FMath::Min(InOutDemand.DistanceCells, Distance);
		InOutDemand.HorizontalDistanceCells = FMath::Min(InOutDemand.HorizontalDistanceCells,
			FVector2D(Delta.X, Delta.Y).Size());
		const double Forward = Distance > UE_DOUBLE_SMALL_NUMBER
			? FVector::DotProduct(InSource.Direction.GetSafeNormal(), Delta / Distance) : 1.0;
		InOutDemand.ForwardScore = FMath::Max(InOutDemand.ForwardScore, Forward);
	}

	bool SameDemandSemantics(const FVoxelExactDemand& InA, const FVoxelExactDemand& InB)
	{
		return InA.bData == InB.bData && InA.bExact == InB.bExact && InA.bCollision == InB.bCollision && InA.bSimulation == InB.bSimulation &&
			   InA.bFineRender == InB.bFineRender && InA.bFineData == InB.bFineData && InA.bWarmupData == InB.bWarmupData &&
			   InA.bWarmupCollision == InB.bWarmupCollision && InA.bMovementCriticalCollision == InB.bMovementCriticalCollision &&
			   InA.bMovementCriticalFine == InB.bMovementCriticalFine && InA.bMovementCriticalData == InB.bMovementCriticalData;
	}

	template<typename KeyType>
	void Difference(const TSet<KeyType>& InPrevious, const TSet<KeyType>& InNext,
		TSet<KeyType>& OutAdded, TSet<KeyType>& OutRemoved)
	{
		for (const KeyType& Key : InNext)
		{
			if (!InPrevious.Contains(Key)) OutAdded.Add(Key);
		}
		for (const KeyType& Key : InPrevious)
		{
			if (!InNext.Contains(Key)) OutRemoved.Add(Key);
		}
	}

	template<typename KeyType>
	bool SameRuntimeSet(const TSet<KeyType>& InA, const TSet<KeyType>& InB)
	{
		if (InA.Num() != InB.Num()) return false;
		for (const KeyType& Key : InA)
		{
			if (!InB.Contains(Key)) return false;
		}
		return true;
	}

	bool SameFinePagePlan(
		const TSharedPtr<const FVoxelFinePagePlan, ESPMode::ThreadSafe>& InA, const TSharedPtr<const FVoxelFinePagePlan, ESPMode::ThreadSafe>& InB)
	{
		if (InA.IsValid() != InB.IsValid())
		{
			return false;
		}
		if (!InA)
		{
			return true;
		}

		return SameRuntimeSet(InA->Pages, InB->Pages) && SameRuntimeMap(InA->SectionOwners, InB->SectionOwners);
	}

	template<typename KeyType>
	bool SamePriorities(const TMap<KeyType, int32>& InA,
		const TMap<KeyType, int32>& InB)
	{
		if (InA.Num() != InB.Num()) return false;
		for (const auto& Pair : InA)
		{
			const int32* Value = InB.Find(Pair.Key);
			if (!Value || *Value != Pair.Value) return false;
		}
		return true;
	}
}

FIntVector FVoxelInterestRuntime::Snap3D(const FIntVector& InCell, const int32 InStep)
{
	return FIntVector(
		VoxelGeneration::FloorDivide(InCell.X, InStep) * InStep,
		VoxelGeneration::FloorDivide(InCell.Y, InStep) * InStep,
		VoxelGeneration::FloorDivide(InCell.Z, InStep) * InStep);
}

FIntPoint FVoxelInterestRuntime::Snap2D(const FIntVector& InCell, const int32 InStep)
{
	return FIntPoint(
		VoxelGeneration::FloorDivide(InCell.X, InStep) * InStep,
		VoxelGeneration::FloorDivide(InCell.Y, InStep) * InStep);
}

EVoxelSourceChange FVoxelInterestRuntime::ClassifySourceChange(const FVoxelStreamingSource& InA,
	const FVoxelStreamingSource& InB, const FVoxelViewSettings& InSettings, const FVoxelSourceInterest* InAccepted)
{
	const FVoxelStreamingSourceView& A = InA.View;
	const FVoxelStreamingSourceView& B = InB.View;
	const EVoxelSourceChange All = EVoxelSourceChange::Priority | EVoxelSourceChange::RuntimeDemand |
		EVoxelSourceChange::FineResidency | EVoxelSourceChange::PageLayout | EVoxelSourceChange::CoarseTopology;
	if (InA.Id != InB.Id || InA.Capabilities != InB.Capabilities || InA.Purpose != InB.Purpose ||
		InA.bInheritWorldView != InB.bInheritWorldView)
	{
		return All;
	}
	EVoxelSourceChange Changed = EVoxelSourceChange::None;
	if (InA.Center != InB.Center || InA.Direction != InB.Direction || InA.Priority != InB.Priority ||
		InA.GetSchedulingPriority() != InB.GetSchedulingPriority() || InA.bLocalView != InB.bLocalView ||
		InA.VelocityCellsPerSecond != InB.VelocityCellsPerSecond)
	{
		Changed |= EVoxelSourceChange::Priority;
	}
	const auto PredictionAnchor = [&InSettings](const FVoxelStreamingSource& Source)
	{
		const FVector Predicted = FVector(Source.Center) + Source.VelocityCellsPerSecond * InSettings.Fine.PredictionSeconds;
		return FIntVector(FMath::FloorToInt(Predicted.X / 16.0), FMath::FloorToInt(Predicted.Y / 16.0), FMath::FloorToInt(Predicted.Z / 16.0));
	};
	if (Snap3D(InA.Center, 16) != Snap3D(InB.Center, 16) || PredictionAnchor(InA) != PredictionAnchor(InB) ||
		InA.ExactRadius != InB.ExactRadius || InA.CollisionRadius != InB.CollisionRadius ||
		InA.SimulationRadius != InB.SimulationRadius ||
		InA.MovementCriticalCollisionRadius != InB.MovementCriticalCollisionRadius ||
		A.FinePredictionSeconds != B.FinePredictionSeconds)
	{
		Changed |= EVoxelSourceChange::RuntimeDemand;
	}
	const FVoxelResolvedViewRanges Steps = VoxelViewLod::ResolveReplanSteps(InB, InSettings, InAccepted);
	if (Snap3D(InA.Center, InSettings.Fine.ShardSideCells * 2) != Snap3D(InB.Center, InSettings.Fine.ShardSideCells * 2))
	{
		Changed |= EVoxelSourceChange::PageLayout;
	}
	if (A.InitialChunkSideCells != B.InitialChunkSideCells)
	{
		Changed |= EVoxelSourceChange::PageLayout | EVoxelSourceChange::CoarseTopology;
	}
	if (InA.VerticalFovDegrees != InB.VerticalFovDegrees || InA.ViewportHeightPixels != InB.ViewportHeightPixels ||
		A.VoxelProxyRadiusCells != B.VoxelProxyRadiusCells ||
		A.VoxelProxyVerticalHalfExtentCells != B.VoxelProxyVerticalHalfExtentCells ||
		A.SurfaceRadiusCells != B.SurfaceRadiusCells || A.MacroRadiusCells != B.MacroRadiusCells ||
		Snap3D(InA.Center, Steps.Proxy) != Snap3D(InB.Center, Steps.Proxy) ||
		Snap2D(InA.Center, Steps.Surface) != Snap2D(InB.Center, Steps.Surface) ||
		Snap2D(InA.Center, Steps.Macro) != Snap2D(InB.Center, Steps.Macro))
	{
		Changed |= EVoxelSourceChange::CoarseTopology;
	}
	return Changed;
}

bool FVoxelInterestRuntime::NeedsUpdate(const FVoxelStreamingSource& InPrevious,
	const FVoxelStreamingSource& InCurrent, const FVoxelViewSettings& InSettings, const FVoxelSourceInterest* InAccepted)
{
	return ClassifySourceChange(InPrevious, InCurrent, InSettings, InAccepted) != EVoxelSourceChange::None;
}

bool FVoxelInterestRuntime::Update(TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest, FVoxelInterestDelta& OutDelta,
	bool& bOutSnapshot, const FVoxelFineResidencyFeedback* InFeedback)
{
	TSharedPtr<FVoxelInterestSet, ESPMode::ThreadSafe> Snapshot;
	const bool bChanged = UpdateSnapshot(InSources, InManifest, InSettings, Snapshot, OutDelta, bOutSnapshot, InFeedback);
	if (Snapshot) OutInterest = *Snapshot;
	else if (LatestInterest) OutInterest = *LatestInterest;
	return bChanged;
}

bool FVoxelInterestRuntime::UpdateSnapshot(TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	TSharedPtr<FVoxelInterestSet, ESPMode::ThreadSafe>& OutInterest, FVoxelInterestDelta& OutDelta,
	bool& bOutSnapshot, const FVoxelFineResidencyFeedback* InFeedback)
{
	OutInterest.Reset();
	static const FVoxelInterestSet EmptyInterest;
	const FVoxelInterestSet& PreviousInterest = LatestInterest ? *LatestInterest : EmptyInterest;
	OutDelta.Reset();
	LastPlanningError.Reset();
	bOutSnapshot = false;
	FString ConfigurationError;
	ON_SCOPE_EXIT
	{
		if (!bOutSnapshot && !ConfigurationError.IsEmpty()) LastPlanningError = ConfigurationError;
	};
	if (!VoxelViewLod::ValidateTopology(InSettings.Lod, ConfigurationError) || !InSettings.Fine.Validate(ConfigurationError))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid terrain policy; retaining previous interest: %s"), *ConfigurationError);
		OutInterest.Reset();
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OutputMetadataLease;
	if (PlanningResources && !PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestSet>(), OutputMetadataLease))
	{ ConfigurationError = TEXT("CapacityBlocked: interest output metadata"); return false; }
	const auto MakeOutput = [&OutputMetadataLease](FVoxelInterestSet&& Value)
	{
		return OutputMetadataLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelInterestSet>(OutputMetadataLease, MoveTemp(Value)) :
			MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>(MoveTemp(Value));
	};
	const bool bPolicyChanged = !bHasSettings || !(LatestSettings.Lod == InSettings.Lod) ||
		!(LatestSettings.Fine == InSettings.Fine) || !(LatestSettings.Readiness == InSettings.Readiness) ||
		!(LatestSettings.Planning == InSettings.Planning);
	static const TMap<FGuid, TSharedPtr<const FSourceState, ESPMode::ThreadSafe>> EmptyStates;
	const auto& States = SourceRoot ? SourceRoot->Values : EmptyStates;
	const int64 MaximumStates = int64(States.Num()) + InSources.Num();
	if (MaximumStates > MAX_int32) { ConfigurationError = TEXT("Oversized: interest Source root"); return false; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StateRootLease, ActiveLease;
	if (PlanningResources)
	{
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FSourceRoot>() +
			VoxelBoundedAllocation::MapBytes<FGuid, TSharedPtr<const FSourceState, ESPMode::ThreadSafe>>(int32(MaximumStates));
		if (Bytes > PlanningResources->Snapshot().Limits.InputBytes)
		{ ConfigurationError = TEXT("Oversized: interest Source root"); return false; }
		if (!PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Input, Bytes, StateRootLease) ||
			!PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Working,
				FMath::Max<uint64>(1, VoxelBoundedAllocation::SetBytes<FGuid>(InSources.Num())), ActiveLease))
		{ ConfigurationError = TEXT("CapacityBlocked: interest Source root"); return false; }
	}
	const auto CandidateRoot = StateRootLease ? VoxelBoundedAllocation::MakeWorkerShared<FSourceRoot>(StateRootLease) :
		MakeShared<FSourceRoot, ESPMode::ThreadSafe>();
	auto& CandidateStates = CandidateRoot->Values;
	CandidateStates.Reserve(int32(MaximumStates));
	for (const auto& Pair : States) CandidateStates.Add(Pair.Key, Pair.Value);
	TSet<FGuid> ActiveIds;
	ActiveIds.Reserve(InSources.Num());
	for (const FVoxelStreamingSource& Source : InSources)
	{
		if (!Source.Id.IsValid() || ActiveIds.Contains(Source.Id) || Source.Direction.ContainsNaN() ||
			Source.VelocityCellsPerSecond.ContainsNaN())
		{
			OutInterest.Reset();
			return false;
		}
		ActiveIds.Add(Source.Id);
	}
	bool bAnyChanged = false;
	bool bPlanChanged = bPolicyChanged || CoarsePlanner.HasRetryReady();
	TSharedPtr<FVoxelFineResidencyLeases, ESPMode::ThreadSafe> CandidateLeases;
	TSharedPtr<FVoxelFinePageLayout, ESPMode::ThreadSafe> CandidateLayout;
	const auto EditLeases = [&]() -> FVoxelFineResidencyLeases*
	{
		if (!CandidateLeases)
		{
			const FVoxelFineResidencyLeases Empty;
			CandidateLeases = (FineLeases ? *FineLeases : Empty).Capture(PlanningResources, PlanningConsumer, ConfigurationError);
		}
		return CandidateLeases.Get();
	};
	const auto EditLayout = [&]() -> FVoxelFinePageLayout*
	{
		if (!CandidateLayout)
		{
			const FVoxelFinePageLayout Empty;
			CandidateLayout = (FineLayout ? *FineLayout : Empty).Capture(PlanningResources, PlanningConsumer, ConfigurationError);
		}
		return CandidateLayout.Get();
	};
	bool bResidencyChanged = false;
	bool bLayoutChanged = false;
	bool bCoarsePlanChanged = bPlanChanged;
	if (bPolicyChanged)
	{
		if (!EditLeases()) return false;
		EditLeases()->Reset();
		if (!EditLayout() || !EditLayout()->Initialize(InSettings.Fine.MaximumPageLevel, InSettings.Fine.ShardSideCells,
			InSettings.Fine.FirstMergeDistanceCells, ConfigurationError))
		{
			OutInterest.Reset();
			return false;
		}
	}
	for (auto Iterator = CandidateStates.CreateIterator(); Iterator; ++Iterator)
	{
		if (!ActiveIds.Contains(Iterator.Key()))
		{
			TVoxelFineSetDelta<FVoxelFineShardKey> ResidencyDelta;
			if (!EditLeases() || !EditLeases()->TryRemoveSource(Iterator.Key(), ResidencyDelta, ConfigurationError) ||
				!EditLayout() || !EditLayout()->ApplyResidencyDelta(ResidencyDelta, ConfigurationError))
			{
				OutInterest.Reset();
				return false;
			}
			if (!EditLayout()->RemoveSource(Iterator.Key(), ConfigurationError)) return false;
			Iterator.RemoveCurrent();
			bAnyChanged = bPlanChanged = bCoarsePlanChanged = bResidencyChanged = bLayoutChanged = true;
		}
	}
	FVoxelInterestManager Manager;
	if (PlanningResources) Manager.BindResources(PlanningResources.ToSharedRef(), PlanningConsumer);
	for (const FVoxelStreamingSource& Source : InSources)
	{
		const auto* PreviousState = CandidateStates.Find(Source.Id);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> StateLease;
		if (PlanningResources && !PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FSourceState>(), StateLease))
		{ ConfigurationError = TEXT("CapacityBlocked: interest Source state"); return false; }
		const auto StateOwner = StateLease ?
			VoxelBoundedAllocation::MakeWorkerShared<FSourceState>(StateLease, PreviousState ? **PreviousState : FSourceState()) :
			MakeShared<FSourceState, ESPMode::ThreadSafe>(PreviousState ? **PreviousState : FSourceState());
		FSourceState& State = *StateOwner;
		if (PlanningResources) State.FineResidency.BindResources(PlanningResources.ToSharedRef(), PlanningConsumer);
		const uint64 AcceptedResidency = State.FineResidency.GetResidencyRevision();
		const bool bHadFineResidency = State.bHasFineResidency;
		const EVoxelSourceChange Change = ClassifySourceChange(State.Source, Source, InSettings, PreviousInterest.Sources.Find(Source.Id));
		bool bRuntimeChanged = bPolicyChanged || EnumHasAnyFlags(Change, EVoxelSourceChange::RuntimeDemand);
		const bool bCoarseChanged = bPolicyChanged || EnumHasAnyFlags(Change, EVoxelSourceChange::CoarseTopology);
		bAnyChanged |= Change != EVoxelSourceChange::None;
		bCoarsePlanChanged |= bCoarseChanged;
		const bool bFine = Source.Has(EVoxelStreamingCapability::FineVisual);
		if (bFine)
		{
			FVoxelFineMovementDelta Movement;
			if (bPolicyChanged || !State.bHasFineResidency)
			{
				if (!State.FineResidency.Initialize(Source.Id, InSettings.Fine.WindowSettings(), Source.Center,
					InSettings.Fine.InteractiveRadiusCells, InSettings.Fine.InteractiveVerticalHalfExtentCells,
					InSettings.Fine.CollisionGuardRadiusCells, InManifest.Settings.MinZ, InManifest.Settings.MaxZ, ConfigurationError))
				{
					OutInterest.Reset();
					return false;
				}
				State.bHasFineResidency = true;
				bRuntimeChanged = true;
				bResidencyChanged = true;
			}
			if (!State.FineResidency.UpdateMovement(Source.Center, Source.VelocityCellsPerSecond, InSettings.Fine.PredictionSeconds,
				Movement, ConfigurationError, false))
			{
				OutInterest.Reset();
				return false;
			}
			bRuntimeChanged |= Movement.bCoreChanged || Movement.bGuardChanged;
			const uint64 PreviousResidency = State.FineResidency.GetResidencyRevision();
			FVoxelFineMigrationDecision Migration;
			if (!State.FineResidency.PrepareWindow(Source.Center, Source.VelocityCellsPerSecond,
				InFeedback ? InFeedback->PreparationSeconds : InSettings.Fine.MinimumPreparationSeconds, Migration, ConfigurationError, false))
			{
				OutInterest.Reset();
				return false;
			}
			bool bTravelBarrier = true;
			if (InFeedback && InFeedback->WorldEpoch != 0 && InFeedback->CoverageRootSerial != 0)
			{
				FVoxelFineSafetyContext Safety;
				Safety.WorldEpoch = InFeedback->WorldEpoch;
				Safety.CoverageRootSerial = InFeedback->CoverageRootSerial;
				Safety.LastAcceptedPosition = InFeedback->LastAcceptedPositions.FindRef(Source.Id);
				Safety.PredictionSeconds = InSettings.Fine.PredictionSeconds;
				const auto Coverage = [InFeedback](const FIntVector& Key)
				{
					return InFeedback->Sections.FindRef(Key);
				};
				FVoxelFineSafetyResult SafetyResult;
				if (InFeedback->LastAcceptedPositions.Contains(Source.Id) &&
					State.FineResidency.EvaluateSafety(Safety, Coverage, SafetyResult, ConfigurationError, false))
				{
					bTravelBarrier = !SafetyResult.IsSafe();
					if (State.FineResidency.GetPending().IsSet() && !bTravelBarrier)
					{
						TVoxelFineSetDelta<FVoxelFineShardKey> Committed;
						State.FineResidency.CommitPending(State.FineResidency.GetPending()->Serial,
							[InFeedback](const FVoxelFineShardKey& Key) { return InFeedback->Shards.FindRef(Key); },
							[InFeedback](const FIntVector& Key) { return InFeedback->Sections.FindRef(Key).bWholeCollisionCommitted; },
							Safety, Coverage, Committed, ConfigurationError, false);
					}
				}
			}
			if (ConfigurationError.StartsWith(TEXT("CapacityBlocked")) || ConfigurationError.StartsWith(TEXT("Oversized"))) return false;
			ConfigurationError.Reset();
			const auto* PreviousSource = PreviousInterest.Sources.Find(Source.Id);
			bPlanChanged |= !PreviousSource || PreviousSource->bRequiresTravelBarrier != bTravelBarrier;
			State.bRequiresTravelBarrier = bTravelBarrier;
			bResidencyChanged |= PreviousResidency != State.FineResidency.GetResidencyRevision();
			if (bPolicyChanged || !bHadFineResidency || AcceptedResidency != State.FineResidency.GetResidencyRevision())
			{
				TVoxelFineSetDelta<FVoxelFineShardKey> ResidencyDelta;
				if (!EditLeases() || !EditLeases()->TrySetSource(Source.Id, State.FineResidency.GetResidentShards(),
					ResidencyDelta, ConfigurationError)) return false;
				bResidencyChanged |= !ResidencyDelta.Added.IsEmpty() || !ResidencyDelta.Removed.IsEmpty();
				if (!EditLayout() || !EditLayout()->ApplyResidencyDelta(ResidencyDelta, ConfigurationError))
				{
					OutInterest.Reset();
					return false;
				}
			}
			if (bPolicyChanged || !bHadFineResidency || EnumHasAnyFlags(Change, EVoxelSourceChange::PageLayout))
			{
				if (!EditLayout() || !EditLayout()->SetSource(Source.Id, Source.Center, ConfigurationError)) return false;
			}
		}
		else if (State.bHasFineResidency)
		{
			TVoxelFineSetDelta<FVoxelFineShardKey> ResidencyDelta;
			if (!EditLeases() || !EditLeases()->TryRemoveSource(Source.Id, ResidencyDelta, ConfigurationError) ||
				!EditLayout() || !EditLayout()->ApplyResidencyDelta(ResidencyDelta, ConfigurationError))
			{
				OutInterest.Reset();
				return false;
			}
			if (!EditLayout()->RemoveSource(Source.Id, ConfigurationError)) return false;
			State.FineResidency.Reset();
			State.bHasFineResidency = false;
			State.bRequiresTravelBarrier = false;
			bResidencyChanged = true;
			bRuntimeChanged = true;
		}
		if (bRuntimeChanged || !State.Source.Id.IsValid())
		{
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ExactLease;
			if (PlanningResources && !PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelInterestSet>(), ExactLease))
			{ ConfigurationError = TEXT("CapacityBlocked: source demand metadata"); return false; }
			const auto Exact = ExactLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelInterestSet>(ExactLease) :
				MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
			Manager.BuildSourceExact(Source, InManifest, InSettings, *Exact, &PreviousInterest,
				State.bHasFineResidency ? &State.FineResidency : nullptr);
			if (!Exact->PlanningError.IsEmpty())
			{
				ConfigurationError = Exact->PlanningError;
				OutInterest.Reset();
				return false;
			}
			State.Exact = Exact;
			bAnyChanged = true;
		}
		const FVoxelResolvedViewRanges Steps = VoxelViewLod::ResolveReplanSteps(Source, InSettings, PreviousInterest.Sources.Find(Source.Id));
		const FIntVector ProxyAnchor = Snap3D(Source.Center, Steps.Proxy);
		const FIntPoint SurfaceAnchor = Snap2D(Source.Center, Steps.Surface);
		const FIntPoint MacroAnchor = Snap2D(Source.Center, Steps.Macro);
		if (bCoarseChanged || State.ProxyAnchor != ProxyAnchor)
		{
			State.ProxyAnchor = ProxyAnchor;
			State.ProxyPlanCenter = Source.Center;
			bCoarsePlanChanged = true;
		}
		if (bCoarseChanged || State.SurfaceAnchor != SurfaceAnchor)
		{
			State.SurfaceAnchor = SurfaceAnchor;
			State.SurfacePlanCenter = Source.Center;
			bCoarsePlanChanged = true;
		}
		if (bCoarseChanged || State.MacroAnchor != MacroAnchor)
		{
			State.MacroAnchor = MacroAnchor;
			State.MacroPlanCenter = Source.Center;
			bCoarsePlanChanged = true;
		}
		bPlanChanged |= bRuntimeChanged || bCoarsePlanChanged || bResidencyChanged;
		State.Source = Source;
		CandidateStates.Add(Source.Id, StateOwner);
	}
	while (CandidateLayout && CandidateLayout->IsPending())
	{
		FVoxelFinePageLayoutDelta LayoutDelta;
		CandidateLayout->Advance(64, LayoutDelta, &ConfigurationError, false);
		if (!ConfigurationError.IsEmpty()) return false;
		bLayoutChanged |= LayoutDelta.bChanged;
	}
	// 布局 Page 的合并、拆分只改变 Fine 呈现；稳定 Shard 驻留变化才修改粗域前驱。
	bCoarsePlanChanged |= bResidencyChanged;
	bPlanChanged |= bCoarsePlanChanged;
	bAnyChanged |= bPlanChanged;

	if (!bAnyChanged)
	{
		return false;
	}
	if (!bPlanChanged)
	{
		FVoxelInterestSet PriorityOnly;
		if (!Manager.CopyForPriority(PreviousInterest, PriorityOnly, ConfigurationError)) return false;
		for (const FVoxelStreamingSource& Source : InSources)
		{
			PriorityOnly.Sources.FindChecked(Source.Id).Source = Source;
		}
		if (!Manager.RefreshPriorities(InSources, InSettings.Fine.InteractiveRadiusCells, PriorityOnly,
			ConfigurationError, PlanningResources, PlanningConsumer))
		{
			OutInterest.Reset();
			return false;
		}
		if (!Manager.BuildPublicationInputs(PriorityOnly, ConfigurationError)) return false;
		check(PrioritySerial < MAX_uint64);
		PriorityOnly.PriorityRevision = ++PrioritySerial;
		SourceRoot = CandidateRoot;
		LatestSettings = InSettings;
		OutInterest = MakeOutput(MoveTemp(PriorityOnly));
		LatestInterest = OutInterest;
		if (LatestInterest && LatestInterest->CoarseCompositionLease) LatestInterest->CoarseCompositionLease->MarkPublished();
		bOutSnapshot = true;
		return false;
	}

	FVoxelInterestSet Prepared;
	const auto* AcceptedLayout = CandidateLayout ? CandidateLayout.Get() : FineLayout.Get();
	int64 ExactRows64 = 0, WarmupRows64 = 0, ShardRows64 = 0, PageShardRows64 = 0;
	uint64 SourceBytes = 0, PageBytes = 0;
	for (const auto& Pair : CandidateStates)
	{
		const auto& State = *Pair.Value;
		ExactRows64 += State.Exact->Exact.Num(); WarmupRows64 += State.Exact->Warmup.Num();
		if (State.bHasFineResidency) ShardRows64 += State.FineResidency.GetResidentShards().Num();
		for (const auto& Source : State.Exact->Sources)
		{
			SourceBytes += VoxelBoundedAllocation::SetBytes<FIntVector>(Source.Value.DataSections.Num()) +
				VoxelBoundedAllocation::SetBytes<FIntVector>(Source.Value.FineDataSections.Num()) +
				VoxelBoundedAllocation::SetBytes<FIntVector>(Source.Value.F0CoreSections.Num()) +
				VoxelBoundedAllocation::SetBytes<FIntVector>(Source.Value.CollisionSections.Num()) +
				VoxelBoundedAllocation::SetBytes<FIntVector>(Source.Value.SimulationSections.Num());
		}
	}
	if (AcceptedLayout) for (const auto& Pair : AcceptedLayout->GetPages())
	{
		PageShardRows64 += Pair.Value.Shards.Num();
		PageBytes += VoxelBoundedAllocation::ArrayBytes<FVoxelFineShardKey>(Pair.Value.Shards.Num());
	}
	if (ExactRows64 > MAX_int32 || WarmupRows64 > MAX_int32 || ShardRows64 > MAX_int32 || PageShardRows64 > MAX_int32)
	{ ConfigurationError = TEXT("Oversized: composed interest rows"); return false; }
	const int32 ExactRows = int32(ExactRows64), WarmupRows = int32(WarmupRows64), ShardRows = int32(ShardRows64);
	const int32 PageCount = AcceptedLayout ? AcceptedLayout->GetPages().Num() : 0;
	if (PlanningResources)
	{
		const uint64 Bytes = SourceBytes + PageBytes + VoxelBoundedAllocation::MapBytes<FGuid, FVoxelSourceInterest>(InSources.Num()) +
			VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelExactDemand>(ExactRows) +
			VoxelBoundedAllocation::MapBytes<FIntVector, FVoxelExactDemand>(WarmupRows) +
			VoxelBoundedAllocation::MapBytes<FVoxelFineShardKey, FVoxelFineShardDemand>(ShardRows) +
			VoxelBoundedAllocation::MapBytes<FVoxelFinePageKey, FVoxelFineLayoutPage>(PageCount) +
			VoxelBoundedAllocation::MapBytes<FVoxelFineShardKey, FVoxelFinePageKey>(int32(PageShardRows64));
		if (Bytes > PlanningResources->Snapshot().Limits.InputBytes)
		{ ConfigurationError = TEXT("Oversized: composed interest containers"); return false; }
		if (!PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Input,
			FMath::Max<uint64>(1, Bytes), Prepared.CompositionLease))
		{ ConfigurationError = TEXT("CapacityBlocked: composed interest containers"); return false; }
	}
	Prepared.Sources.Reserve(InSources.Num()); Prepared.Exact.Reserve(ExactRows); Prepared.Warmup.Reserve(WarmupRows);
	Prepared.FineShardDemands.Reserve(ShardRows); Prepared.FineShardPages.Reserve(PageCount); Prepared.FineShardOwners.Reserve(int32(PageShardRows64));
	for (const FVoxelStreamingSource& Source : InSources)
	{
		const FSourceState& State = *CandidateStates.FindChecked(Source.Id);
		FVoxelSourceInterest& SourceInterest = Prepared.Sources.FindOrAdd(Source.Id);
		SourceInterest.Source = Source;
		SourceInterest.bHasStageCenters = true;
		SourceInterest.StageCenters[0] = State.ProxyPlanCenter;
		SourceInterest.StageCenters[1] = State.SurfacePlanCenter;
		SourceInterest.StageCenters[2] = State.MacroPlanCenter;
		for (const FVoxelInterestSet* Layer : { State.Exact.Get() })
		{
			if (const FVoxelSourceInterest* Part = Layer->Sources.Find(Source.Id))
			{
				SourceInterest.DataSections.Reserve(Part->DataSections.Num());
				SourceInterest.FineDataSections.Reserve(Part->FineDataSections.Num());
				SourceInterest.F0CoreSections.Reserve(Part->F0CoreSections.Num());
				SourceInterest.CollisionSections.Reserve(Part->CollisionSections.Num());
				SourceInterest.SimulationSections.Reserve(Part->SimulationSections.Num());
				for (const FIntVector& Key : Part->DataSections) SourceInterest.DataSections.Add(Key);
				for (const FIntVector& Key : Part->FineDataSections) SourceInterest.FineDataSections.Add(Key);
				for (const FIntVector& Key : Part->F0CoreSections) SourceInterest.F0CoreSections.Add(Key);
				for (const FIntVector& Key : Part->CollisionSections) SourceInterest.CollisionSections.Add(Key);
				for (const FIntVector& Key : Part->SimulationSections) SourceInterest.SimulationSections.Add(Key);
			}
			for (const auto& Pair : Layer->Exact)
			{
				MergeDemand(Prepared.Exact.FindOrAdd(Pair.Key), Pair.Value, Source, Pair.Key);
			}
			for (const auto& Pair : Layer->Warmup)
			{
				FVoxelExactDemand& Demand = Prepared.Warmup.FindOrAdd(Pair.Key);
				Demand.bWarmupData |= Pair.Value.bWarmupData;
				Demand.bWarmupCollision |= Pair.Value.bWarmupCollision;
				Demand.bFineRender |= Pair.Value.bFineRender;
			}
		}
		if (State.bHasFineResidency)
		{
			if (!Manager.BuildFineShardDemand(Source, State.FineResidency, Prepared, ConfigurationError))
			{
				OutInterest.Reset();
				return false;
			}
			SourceInterest.bRequiresTravelBarrier = State.bRequiresTravelBarrier;
		}
	}
	if (AcceptedLayout) for (const auto& Pair : AcceptedLayout->GetPages()) Prepared.FineShardPages.Add(Pair.Key, Pair.Value);
	for (const auto& Pair : Prepared.FineShardPages)
	{
		for (const FVoxelFineShardKey& Shard : Pair.Value.Shards) Prepared.FineShardOwners.Add(Shard, Pair.Key);
	}
	FVoxelInterestSet Next = Manager.Finalize(InSources, InManifest, InSettings, MoveTemp(Prepared), &PreviousInterest, bCoarsePlanChanged, &CoarsePlanner);
	for (auto& Pair : Next.FineShardDemands)
	{
		const auto* Previous = PreviousInterest.FineShardDemands.Find(Pair.Key);
		Pair.Value.FirstQueuedSeconds = Previous ? Previous->FirstQueuedSeconds :
			PlanningFirstQueuedSeconds > 0.0 ? PlanningFirstQueuedSeconds : FPlatformTime::Seconds();
		Pair.Value.QueueSequence = Previous ? Previous->QueueSequence : ++FineDemandSequence;
	}
	if (!Next.PlanningError.IsEmpty())
	{
		const bool bPlanningPending = Next.PlanningError.StartsWith(TEXT("PlanningPending"));
		if (bPlanningPending) LastPlanningError.Reset();
		else
		{
			LastPlanningError = Next.PlanningError;
			UE_LOG(LogTemp, Error, TEXT("Terrain plan failed; preserving committed interest: %s"), *Next.PlanningError);
		}
		OutInterest.Reset();
		return false;
	}
	if (PlanningResources)
	{
		const uint64 Bytes = 3 * VoxelBoundedAllocation::SetBytes<FIntVector>(PreviousInterest.Exact.Num()) +
			3 * VoxelBoundedAllocation::SetBytes<FIntVector>(Next.Exact.Num()) +
			VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(Next.FineShardDemands.Num()) +
			VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(PreviousInterest.FineShardDemands.Num()) +
			VoxelBoundedAllocation::SetBytes<FVoxelFineShardKey>(FMath::Min(Next.FineShardDemands.Num(), PreviousInterest.FineShardDemands.Num())) +
			VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(Next.VoxelProxy.Num()) + VoxelBoundedAllocation::SetBytes<FVoxelViewKey>(PreviousInterest.VoxelProxy.Num()) +
			VoxelBoundedAllocation::SetBytes<FVoxelSurfaceTileKey>(Next.Surface.Num()) + VoxelBoundedAllocation::SetBytes<FVoxelSurfaceTileKey>(PreviousInterest.Surface.Num()) +
			VoxelBoundedAllocation::SetBytes<FVoxelMacroTileKey>(Next.Macro.Num()) + VoxelBoundedAllocation::SetBytes<FVoxelMacroTileKey>(PreviousInterest.Macro.Num()) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainSpatialKey>(Next.Geometry.Num());
		if (Bytes > PlanningResources->Snapshot().Limits.InputBytes)
		{ ConfigurationError = TEXT("Oversized: interest delta containers"); return false; }
		if (!PlanningResources->Acquire(PlanningConsumer, EVoxelTerrainResourcePool::Input, FMath::Max<uint64>(1, Bytes), OutDelta.ResourceLease))
		{ ConfigurationError = TEXT("CapacityBlocked: interest delta containers"); return false; }
	}
	OutDelta.AddedExact.Reserve(Next.Exact.Num()); OutDelta.RemovedExact.Reserve(PreviousInterest.Exact.Num());
	OutDelta.AddedFine.Reserve(Next.Exact.Num()); OutDelta.RemovedFine.Reserve(PreviousInterest.Exact.Num());
	OutDelta.AddedSimulation.Reserve(Next.Exact.Num());
	OutDelta.RemovedSimulation.Reserve(PreviousInterest.Exact.Num());
	for (const auto& Pair : Next.Exact)
	{
		const auto* Previous = PreviousInterest.Exact.Find(Pair.Key);
		if (!Previous) OutDelta.AddedExact.Add(Pair.Key);
		if (Pair.Value.bSimulation && (!Previous || !Previous->bSimulation)) OutDelta.AddedSimulation.Add(Pair.Key);
		if (Pair.Value.bFineRender && (!Previous || !Previous->bFineRender)) OutDelta.AddedFine.Add(Pair.Key);
	}
	for (const auto& Pair : PreviousInterest.Exact)
	{
		const auto* Desired = Next.Exact.Find(Pair.Key);
		if (!Desired) OutDelta.RemovedExact.Add(Pair.Key);
		if (Pair.Value.bSimulation && (!Desired || !Desired->bSimulation)) OutDelta.RemovedSimulation.Add(Pair.Key);
		if (Pair.Value.bFineRender && (!Desired || !Desired->bFineRender)) OutDelta.RemovedFine.Add(Pair.Key);
	}
	OutDelta.FineResidency.Added.Reserve(Next.FineShardDemands.Num());
	OutDelta.FineResidency.Removed.Reserve(PreviousInterest.FineShardDemands.Num());
	OutDelta.FineResidency.Retained.Reserve(FMath::Min(Next.FineShardDemands.Num(), PreviousInterest.FineShardDemands.Num()));
	for (const auto& Pair : Next.FineShardDemands)
	{
		if (PreviousInterest.FineShardDemands.Contains(Pair.Key)) OutDelta.FineResidency.Retained.Add(Pair.Key);
		else OutDelta.FineResidency.Added.Add(Pair.Key);
	}
	for (const auto& Pair : PreviousInterest.FineShardDemands)
		if (!Next.FineShardDemands.Contains(Pair.Key)) OutDelta.FineResidency.Removed.Add(Pair.Key);
	OutDelta.AddedProxy.Reserve(Next.VoxelProxy.Num()); OutDelta.RemovedProxy.Reserve(PreviousInterest.VoxelProxy.Num());
	OutDelta.AddedSurface.Reserve(Next.Surface.Num()); OutDelta.RemovedSurface.Reserve(PreviousInterest.Surface.Num());
	OutDelta.AddedMacro.Reserve(Next.Macro.Num()); OutDelta.RemovedMacro.Reserve(PreviousInterest.Macro.Num());
	OutDelta.ChangedGeometry.Reserve(Next.Geometry.Num());
	OutDelta.bFineResidencyChanged = bResidencyChanged;
	OutDelta.bPageLayoutChanged = bLayoutChanged;
	Difference(PreviousInterest.VoxelProxy, Next.VoxelProxy, OutDelta.AddedProxy, OutDelta.RemovedProxy);
	Difference(PreviousInterest.Surface, Next.Surface, OutDelta.AddedSurface, OutDelta.RemovedSurface);
	Difference(PreviousInterest.Macro, Next.Macro, OutDelta.AddedMacro, OutDelta.RemovedMacro);
	OutDelta.bVisualPartitionChanged = !OutDelta.AddedFine.IsEmpty() || !OutDelta.RemovedFine.IsEmpty() ||
		!OutDelta.AddedProxy.IsEmpty() || !OutDelta.RemovedProxy.IsEmpty() ||
		!OutDelta.AddedSurface.IsEmpty() || !OutDelta.RemovedSurface.IsEmpty() ||
		!OutDelta.AddedMacro.IsEmpty() || !OutDelta.RemovedMacro.IsEmpty() || bResidencyChanged || bLayoutChanged;
	for (const auto& Pair : Next.Geometry)
	{
		const auto* PreviousGeometry = PreviousInterest.Geometry.Find(Pair.Key);
		if (!PreviousGeometry || *PreviousGeometry != Pair.Value)
		{ OutDelta.ChangedGeometry.Add(Pair.Key); OutDelta.bVisualPartitionChanged = true; }
	}
	OutDelta.bVisualPartitionChanged |= !SameFinePagePlan(PreviousInterest.FinePagePlan, Next.FinePagePlan) ||
										!SameRuntimeSet(PreviousInterest.TerrainPlan.Roots, Next.TerrainPlan.Roots) ||
										!SameRuntimeSet(PreviousInterest.TerrainPlan.Leaves, Next.TerrainPlan.Leaves) ||
										!SameRuntimeSet(PreviousInterest.TerrainPlan.FineDependencies, Next.TerrainPlan.FineDependencies);
	bool bChanged = !OutDelta.AddedExact.IsEmpty() || !OutDelta.RemovedExact.IsEmpty() ||
		OutDelta.bVisualPartitionChanged || Next.Warmup.Num() != PreviousInterest.Warmup.Num() ||
		Next.Sources.Num() != PreviousInterest.Sources.Num() ||
		!SamePriorities(Next.VoxelProxyPriorities, PreviousInterest.VoxelProxyPriorities) ||
		!SamePriorities(Next.SurfacePriorities, PreviousInterest.SurfacePriorities) ||
		!SamePriorities(Next.MacroPriorities, PreviousInterest.MacroPriorities);
	if (!bChanged)
	{
		bChanged = Next.PlayableFineKeys.Num() != PreviousInterest.PlayableFineKeys.Num();
		for (const FIntVector& Key : Next.PlayableFineKeys)
		{
			if (!PreviousInterest.PlayableFineKeys.Contains(Key)) { bChanged = true; break; }
		}
	}
	if (!bChanged)
	{
		for (const auto& Pair : Next.Sources)
		{
			const FVoxelSourceInterest* Previous = PreviousInterest.Sources.Find(Pair.Key);
			if (!Previous || !SameRuntimeSet(Pair.Value.DataSections, Previous->DataSections) ||
				!SameRuntimeSet(Pair.Value.FineDataSections, Previous->FineDataSections) ||
				!SameRuntimeSet(Pair.Value.CollisionSections, Previous->CollisionSections))
			{
				bChanged = true;
				break;
			}
		}
	}
	if (!bChanged)
	{
		for (const auto& Pair : Next.Warmup)
		{
			const FVoxelExactDemand* Previous = PreviousInterest.Warmup.Find(Pair.Key);
			if (!Previous || !SameDemandSemantics(*Previous, Pair.Value))
			{
				bChanged = true;
				break;
			}
		}
	}
	if (!bChanged)
	{
		for (const auto& Pair : Next.Exact)
		{
			const FVoxelExactDemand* Previous = PreviousInterest.Exact.Find(Pair.Key);
			if (!Previous || !SameDemandSemantics(*Previous, Pair.Value))
			{
				bChanged = true;
				break;
			}
		}
	}
	bool bDemandChanged = Next.Exact.Num() != PreviousInterest.Exact.Num() || Next.Warmup.Num() != PreviousInterest.Warmup.Num();
	bool bPriorityChanged = !SamePriorities(Next.VoxelProxyPriorities, PreviousInterest.VoxelProxyPriorities) ||
							!SamePriorities(Next.SurfacePriorities, PreviousInterest.SurfacePriorities) ||
							!SamePriorities(Next.MacroPriorities, PreviousInterest.MacroPriorities);
	for (const auto& Pair : Next.Exact)
	{
		const auto* Previous = PreviousInterest.Exact.Find(Pair.Key);
		bDemandChanged |= !Previous || !SameDemandSemantics(*Previous, Pair.Value);
		bPriorityChanged |= !Previous || Previous->ServicePriority != Pair.Value.ServicePriority;
	}
	bPriorityChanged |= !SameRuntimeMap(Next.GeometryServicePriorities, PreviousInterest.GeometryServicePriorities) ||
		!SameRuntimeMap(Next.FinePageServicePriorities, PreviousInterest.FinePageServicePriorities);
	const auto NextRevision = [](const uint64 Revision, const bool bChanged)
	{
		check(!bChanged || Revision < MAX_uint64);
		return bChanged ? Revision + 1 : Revision;
	};
	Next.PublicationRevision = PublicationSerial = NextRevision(PublicationSerial, OutDelta.bVisualPartitionChanged || PublicationSerial == 0);
	Next.DemandRevision = DemandSerial = NextRevision(DemandSerial, bDemandChanged || DemandSerial == 0);
	Next.PriorityRevision = PrioritySerial = NextRevision(PrioritySerial, bPriorityChanged || PrioritySerial == 0);
	Next.ResidencyRevision = ResidencySerial = NextRevision(ResidencySerial, bResidencyChanged || ResidencySerial == 0);
	Next.LayoutRevision = LayoutSerial = NextRevision(LayoutSerial, bLayoutChanged || LayoutSerial == 0);
	OutDelta.bRuntimeDemandChanged = bDemandChanged;
	bChanged |= bDemandChanged || bResidencyChanged || bLayoutChanged;
	LatestSettings = InSettings;
	bHasSettings = true;
	SourceRoot = CandidateRoot;
	if (CandidateLeases) FineLeases = MoveTemp(CandidateLeases);
	if (CandidateLayout) FineLayout = MoveTemp(CandidateLayout);
	OutInterest = MakeOutput(MoveTemp(Next));
	LatestInterest = OutInterest;
	if (LatestInterest && LatestInterest->CoarseCompositionLease) LatestInterest->CoarseCompositionLease->MarkPublished();
	bOutSnapshot = true;
	return bChanged;
}

void FVoxelInterestRuntime::Reset()
{
	SourceRoot.Reset();
	FineLeases.Reset();
	FineLayout.Reset();
	CoarsePlanner.Reset();
	LastPlanningError.Reset();
	LatestInterest = {};
	LatestSettings = {};
	FineDemandSequence = 0;
	bHasSettings = false;
}
