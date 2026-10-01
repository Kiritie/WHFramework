#include "Voxel/Streaming/VoxelInterestRuntime.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Streaming/VoxelInterestManager.h"

namespace
{
	constexpr int32 RuntimeInterestSectionSide = 16;

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
		return InA.bData == InB.bData && InA.bExact == InB.bExact &&
			InA.bCollision == InB.bCollision && InA.bSimulation == InB.bSimulation &&
			InA.bFineRender == InB.bFineRender && InA.bFineData == InB.bFineData &&
			InA.bWarmupData == InB.bWarmupData && InA.bWarmupCollision == InB.bWarmupCollision &&
			InA.bMovementCriticalCollision == InB.bMovementCriticalCollision &&
			InA.bMovementCriticalFine == InB.bMovementCriticalFine &&
			InA.bMovementCriticalData == InB.bMovementCriticalData && InA.Priority == InB.Priority;
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

bool FVoxelInterestRuntime::ConfigurationChanged(const FVoxelStreamingSource& InA,
	const FVoxelStreamingSource& InB)
{
	const FVoxelStreamingSourceView& A = InA.View;
	const FVoxelStreamingSourceView& B = InB.View;
	return InA.Capabilities != InB.Capabilities || InA.Purpose != InB.Purpose ||
		InA.bLocalView != InB.bLocalView ||
		InA.bInheritWorldView != InB.bInheritWorldView ||
		InA.RetentionRadiusCells != InB.RetentionRadiusCells ||
		InA.Priority != InB.Priority || InA.GetSchedulingPriority() != InB.GetSchedulingPriority() ||
		InA.ExactRadius != InB.ExactRadius || InA.CollisionRadius != InB.CollisionRadius ||
		InA.SimulationRadius != InB.SimulationRadius ||
		InA.VerticalExactRadius != InB.VerticalExactRadius ||
		InA.MovementCriticalCollisionRadius != InB.MovementCriticalCollisionRadius ||
		InA.VerticalFovDegrees != InB.VerticalFovDegrees ||
		InA.ViewportHeightPixels != InB.ViewportHeightPixels ||
		A.WarmupDataRadiusCells != B.WarmupDataRadiusCells ||
		A.WarmupCollisionRadiusCells != B.WarmupCollisionRadiusCells ||
		A.FineRadiusCells != B.FineRadiusCells ||
		A.FineVerticalRadiusCells != B.FineVerticalRadiusCells ||
		A.FinePreloadCells != B.FinePreloadCells ||
		A.VoxelProxyRadiusCells != B.VoxelProxyRadiusCells ||
		A.SurfaceRadiusCells != B.SurfaceRadiusCells ||
		A.TargetScreenErrorPixels != B.TargetScreenErrorPixels ||
		A.MaximumVoxelProxyLevel != B.MaximumVoxelProxyLevel ||
		A.MaximumSurfaceLevel != B.MaximumSurfaceLevel ||
		A.FineReplanCells != B.FineReplanCells ||
		A.ProxyReplanCells != B.ProxyReplanCells ||
		A.SurfaceReplanCells != B.SurfaceReplanCells ||
		A.MacroReplanCells != B.MacroReplanCells ||
		A.FinePredictionSeconds != B.FinePredictionSeconds ||
		A.MovementCriticalFineRadiusCells != B.MovementCriticalFineRadiusCells;
}

bool FVoxelInterestRuntime::NeedsUpdate(const FVoxelStreamingSource& InPrevious,
	const FVoxelStreamingSource& InCurrent)
{
	if (ConfigurationChanged(InPrevious, InCurrent)) return true;
	const FIntVector& Previous = InPrevious.Center;
	const FIntVector& Current = InCurrent.Center;
	const FVoxelStreamingSourceView& View = InCurrent.View;
	if (Snap3D(Previous, FMath::Clamp(InCurrent.CollisionRadius / 2, 4, 16)) !=
		Snap3D(Current, FMath::Clamp(InCurrent.CollisionRadius / 2, 4, 16)) ||
		Snap3D(Previous, FMath::Max(16, View.FineReplanCells)) !=
		Snap3D(Current, FMath::Max(16, View.FineReplanCells)) ||
		Snap3D(Previous, FMath::Max(16, View.ProxyReplanCells)) !=
		Snap3D(Current, FMath::Max(16, View.ProxyReplanCells)) ||
		Snap2D(Previous, FMath::Max(32, View.SurfaceReplanCells)) !=
		Snap2D(Current, FMath::Max(32, View.SurfaceReplanCells)) ||
		Snap2D(Previous, FMath::Max(256, View.MacroReplanCells)) !=
		Snap2D(Current, FMath::Max(256, View.MacroReplanCells)))
	{
		return true;
	}
	const FVector PreviousPrediction = FVector(Previous) +
		InPrevious.VelocityCellsPerSecond * View.FinePredictionSeconds;
	const FVector CurrentPrediction = FVector(Current) +
		InCurrent.VelocityCellsPerSecond * View.FinePredictionSeconds;
	const FIntVector PreviousPredicted(FMath::RoundToInt(PreviousPrediction.X),
		FMath::RoundToInt(PreviousPrediction.Y), FMath::RoundToInt(PreviousPrediction.Z));
	const FIntVector CurrentPredicted(FMath::RoundToInt(CurrentPrediction.X),
		FMath::RoundToInt(CurrentPrediction.Y), FMath::RoundToInt(CurrentPrediction.Z));
	return Snap3D(PreviousPredicted, RuntimeInterestSectionSide) !=
		Snap3D(CurrentPredicted, RuntimeInterestSectionSide) ||
		FVector::DotProduct(InPrevious.Direction.GetSafeNormal(),
			InCurrent.Direction.GetSafeNormal()) < 0.9;
}

bool FVoxelInterestRuntime::Update(TConstArrayView<FVoxelStreamingSource> InSources,
	const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
	FVoxelInterestSet& OutInterest, FVoxelInterestDelta& OutDelta,
	bool& bOutSnapshot)
{
	OutDelta.Reset();
	bOutSnapshot = false;
	TSet<FGuid> ActiveIds;
	for (const FVoxelStreamingSource& Source : InSources) ActiveIds.Add(Source.Id);
	bool bAnyChanged = false;
	for (auto Iterator = States.CreateIterator(); Iterator; ++Iterator)
	{
		if (!ActiveIds.Contains(Iterator.Key()))
		{
			Iterator.RemoveCurrent();
			bAnyChanged = true;
		}
	}
	const FVoxelInterestManager Manager;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		FSourceState& State = States.FindOrAdd(Source.Id);
		const bool bConfigurationChanged = ConfigurationChanged(State.Source, Source);
		const FIntVector ExactAnchor = Snap3D(Source.Center,
			FMath::Clamp(Source.CollisionRadius / 2, 4, 16));
		const FIntVector FineAnchor = Snap3D(Source.Center,
			FMath::Max(16, Source.View.FineReplanCells));
		const FIntVector ProxyAnchor = Snap3D(Source.Center,
			FMath::Max(16, Source.View.ProxyReplanCells));
		const FIntPoint SurfaceAnchor = Snap2D(Source.Center,
			FMath::Max(32, Source.View.SurfaceReplanCells));
		const FIntPoint MacroAnchor = Snap2D(Source.Center,
			FMath::Max(256, Source.View.MacroReplanCells));
		const FVector Predicted = FVector(Source.Center) +
			Source.VelocityCellsPerSecond * Source.View.FinePredictionSeconds;
		const FIntVector PredictedCenter(FMath::RoundToInt(Predicted.X),
			FMath::RoundToInt(Predicted.Y), FMath::RoundToInt(Predicted.Z));
		const FIntVector PredictionAnchor = Snap3D(PredictedCenter, RuntimeInterestSectionSide);
		if (bConfigurationChanged || State.ExactAnchor != ExactAnchor)
		{
			State.Exact = {};
			FVoxelStreamingSource ExactSource = Source;
			ExactSource.Capabilities &= ~(EVoxelStreamingCapability::FineVisual |
				EVoxelStreamingCapability::WorldVisual |
				EVoxelStreamingCapability::LocalRefinement);
			ExactSource.View.FineRadiusCells = 0;
			Manager.BuildSourceExact(ExactSource, InManifest, InSettings, State.Exact,
				&LatestInterest);
			State.ExactAnchor = ExactAnchor;
			bAnyChanged = true;
		}
		if (bConfigurationChanged || State.FineAnchor != FineAnchor)
		{
			State.Fine = {};
			if (Source.Has(EVoxelStreamingCapability::FineVisual))
			{
				FVoxelStreamingSource FineSource = Source;
				FineSource.Capabilities = EVoxelStreamingCapability::Data |
					EVoxelStreamingCapability::FineVisual;
				if (Source.Has(EVoxelStreamingCapability::LocalRefinement))
				{
					FineSource.Capabilities |= EVoxelStreamingCapability::LocalRefinement;
				}
				FineSource.ExactRadius = -1;
				FineSource.CollisionRadius = 0;
				FineSource.SimulationRadius = 0;
				FineSource.VerticalExactRadius = 0;
				FineSource.MovementCriticalCollisionRadius = 0;
				FineSource.View.WarmupDataRadiusCells = 0;
				FineSource.View.WarmupCollisionRadiusCells = 0;
				Manager.BuildSourceExact(FineSource, InManifest, InSettings, State.Fine,
					&LatestInterest);
			}
			State.FineAnchor = FineAnchor;
			bAnyChanged = true;
		}
		if (bConfigurationChanged || State.ProxyAnchor != ProxyAnchor)
		{
			State.Proxy = {};
			Manager.BuildSourceProxy(Source, InManifest, InSettings, State.Proxy);
			State.ProxyAnchor = ProxyAnchor;
			bAnyChanged = true;
		}
		if (bConfigurationChanged || State.SurfaceAnchor != SurfaceAnchor)
		{
			State.Surface = {};
			Manager.BuildSourceSurface(Source, InManifest, InSettings, State.Surface);
			State.SurfaceAnchor = SurfaceAnchor;
			bAnyChanged = true;
		}
		if (bConfigurationChanged || State.MacroAnchor != MacroAnchor)
		{
			State.Macro = {};
			Manager.BuildSourceMacro(Source, InManifest, InSettings, State.Macro);
			State.MacroAnchor = MacroAnchor;
			bAnyChanged = true;
		}
		if (State.PredictionAnchor != PredictionAnchor ||
			FVector::DotProduct(State.Source.Direction.GetSafeNormal(),
				Source.Direction.GetSafeNormal()) < 0.9)
		{
			bAnyChanged = true;
		}
		State.PredictionAnchor = PredictionAnchor;
		State.Source = Source;
	}
	if (!bAnyChanged)
	{
		return false;
	}

	FVoxelInterestSet Prepared;
	for (const FVoxelStreamingSource& Source : InSources)
	{
		const FSourceState& State = States.FindChecked(Source.Id);
		FVoxelSourceInterest& SourceInterest = Prepared.Sources.FindOrAdd(Source.Id);
		SourceInterest.Source = Source;
		for (const FVoxelInterestSet* Layer : { &State.Exact, &State.Fine })
		{
			if (const FVoxelSourceInterest* Part = Layer->Sources.Find(Source.Id))
			{
				for (const FIntVector& Key : Part->DataSections) SourceInterest.DataSections.Add(Key);
				for (const FIntVector& Key : Part->FineDataSections) SourceInterest.FineDataSections.Add(Key);
				for (const FIntVector& Key : Part->CollisionSections) SourceInterest.CollisionSections.Add(Key);
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
		for (const FVoxelViewKey& Key : State.Proxy.VoxelProxy) Prepared.VoxelProxy.Add(Key);
		for (const FVoxelSurfaceTileKey& Key : State.Surface.Surface) Prepared.Surface.Add(Key);
		for (const FVoxelMacroTileKey& Key : State.Macro.Macro) Prepared.Macro.Add(Key);
	}
	FVoxelInterestSet Next = Manager.Finalize(InSources, InManifest, InSettings, MoveTemp(Prepared));
	TSet<FIntVector> PreviousExact;
	TSet<FIntVector> NextExact;
	TSet<FIntVector> PreviousFine;
	TSet<FIntVector> NextFine;
	for (const auto& Pair : LatestInterest.Exact)
	{
		PreviousExact.Add(Pair.Key);
		if (Pair.Value.bFineRender) PreviousFine.Add(Pair.Key);
	}
	for (const auto& Pair : Next.Exact)
	{
		NextExact.Add(Pair.Key);
		if (Pair.Value.bFineRender) NextFine.Add(Pair.Key);
	}
	Difference(PreviousExact, NextExact, OutDelta.AddedExact, OutDelta.RemovedExact);
	Difference(PreviousFine, NextFine, OutDelta.AddedFine, OutDelta.RemovedFine);
	Difference(LatestInterest.VoxelProxy, Next.VoxelProxy, OutDelta.AddedProxy, OutDelta.RemovedProxy);
	Difference(LatestInterest.Surface, Next.Surface, OutDelta.AddedSurface, OutDelta.RemovedSurface);
	Difference(LatestInterest.Macro, Next.Macro, OutDelta.AddedMacro, OutDelta.RemovedMacro);
	OutDelta.bVisualPartitionChanged = !OutDelta.AddedFine.IsEmpty() || !OutDelta.RemovedFine.IsEmpty() ||
		!OutDelta.AddedProxy.IsEmpty() || !OutDelta.RemovedProxy.IsEmpty() ||
		!OutDelta.AddedSurface.IsEmpty() || !OutDelta.RemovedSurface.IsEmpty() ||
		!OutDelta.AddedMacro.IsEmpty() || !OutDelta.RemovedMacro.IsEmpty();
	bool bChanged = !OutDelta.AddedExact.IsEmpty() || !OutDelta.RemovedExact.IsEmpty() ||
		OutDelta.bVisualPartitionChanged || Next.Warmup.Num() != LatestInterest.Warmup.Num() ||
		Next.Sources.Num() != LatestInterest.Sources.Num() ||
		!SamePriorities(Next.VoxelProxyPriorities, LatestInterest.VoxelProxyPriorities) ||
		!SamePriorities(Next.SurfacePriorities, LatestInterest.SurfacePriorities) ||
		!SamePriorities(Next.MacroPriorities, LatestInterest.MacroPriorities);
	if (!bChanged)
	{
		TSet<FIntVector> NextPlayable;
		TSet<FIntVector> PreviousPlayable;
		NextPlayable.Append(Next.PlayableFineKeys);
		PreviousPlayable.Append(LatestInterest.PlayableFineKeys);
		bChanged = !SameRuntimeSet(NextPlayable, PreviousPlayable);
	}
	if (!bChanged)
	{
		for (const auto& Pair : Next.Sources)
		{
			const FVoxelSourceInterest* Previous = LatestInterest.Sources.Find(Pair.Key);
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
			const FVoxelExactDemand* Previous = LatestInterest.Warmup.Find(Pair.Key);
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
			const FVoxelExactDemand* Previous = LatestInterest.Exact.Find(Pair.Key);
			if (!Previous || !SameDemandSemantics(*Previous, Pair.Value))
			{
				bChanged = true;
				break;
			}
		}
	}
	LatestInterest = MoveTemp(Next);
	OutInterest = LatestInterest;
	bOutSnapshot = true;
	return bChanged;
}

void FVoxelInterestRuntime::Reset()
{
	States.Reset();
	LatestInterest = {};
}
