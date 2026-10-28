#include "Voxel/Runtime/VoxelChangeHierarchy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

uint64 FVoxelChangeHierarchySnapshot::GetSectionRevision(const FIntVector& InSection) const
{
	const auto Record = SectionChanges.Find({EVoxelTerrainRepresentation::Fine, InSection, 0});
	return Record ? Record->Stamp.ContentRevision : 0;
}

uint64 FVoxelChangeHierarchySnapshot::GetFineShardRevision(const FVoxelFineShardKey& InKey) const
{
	const auto Bounds = InKey.Bounds();
	const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
	const int64 Horizontal = int64(NaturalHorizontalInfluence) + 1;
	const FIntVector Min(Cell(int64(Bounds.Min.X) - Horizontal), Cell(int64(Bounds.Min.Y) - Horizontal),
		Cell(int64(Bounds.Min.Z) - 1 - NaturalUpwardInfluence));
	const FIntVector Max(Cell(int64(Bounds.Max.X) + Horizontal), Cell(int64(Bounds.Max.Y) + Horizontal), Cell(int64(Bounds.Max.Z) + 1));
	uint64 Revision = 0;
	SectionChanges.AnyRecord(Min, Max, [&Revision](const FVoxelCoverageRecord& Record)
	{
		Revision = FMath::Max(Revision, Record.Stamp.ContentRevision);
		return false;
	});
	return Revision;
}

uint64 FVoxelChangeHierarchySnapshot::GetGeometryRevision(const FVoxelTerrainGeometry& InGeometry) const
{
	FIntVector Min;
	FIntVector Max;
	if (!InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Min, Max)) return 0;
	const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
	const int64 Step = InGeometry.SampleStepCells;
	const int64 Horizontal = Step + NaturalHorizontalInfluence;
	uint64 Revision = 0;
	SectionChanges.AnyRecord(FIntVector(Cell(int64(Min.X) - Horizontal), Cell(int64(Min.Y) - Horizontal),
		Cell(int64(Min.Z) - Step - NaturalUpwardInfluence)),
		FIntVector(Cell(int64(Max.X) + Horizontal), Cell(int64(Max.Y) + Horizontal), Cell(int64(Max.Z) + Step)),
		[&Revision](const FVoxelCoverageRecord& Record)
		{
			Revision = FMath::Max(Revision, Record.Stamp.ContentRevision);
			return false;
		});
	return Revision;
}

FVoxelChangeHierarchySnapshot FVoxelChangeHierarchy::CaptureVersionSnapshot() const
{
	FReadScopeLock Scope(Lock);
	return {SectionChangeIndex, RevisionSerial, NaturalHorizontalInfluence, NaturalUpwardInfluence};
}

bool FVoxelChangeHierarchy::IsVersionSnapshotCurrent(const FVoxelChangeHierarchySnapshot& InSnapshot,
	const FVoxelGenerationBounds& InBounds, const int32 InHaloStep) const
{
	FReadScopeLock Scope(Lock);
	if (!InBounds.IsValid() || InHaloStep < 1 || NaturalHorizontalInfluence != InSnapshot.NaturalHorizontalInfluence ||
		NaturalUpwardInfluence != InSnapshot.NaturalUpwardInfluence) return false;
	const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
	const int64 Horizontal = int64(NaturalHorizontalInfluence) + InHaloStep;
	return SectionChangeIndex.IsRangeCurrent(InSnapshot.SectionChanges,
		FIntVector(Cell(int64(InBounds.Min.X) - Horizontal), Cell(int64(InBounds.Min.Y) - Horizontal),
			Cell(int64(InBounds.Min.Z) - InHaloStep - NaturalUpwardInfluence)),
		FIntVector(Cell(int64(InBounds.Max.X) + Horizontal), Cell(int64(InBounds.Max.Y) + Horizontal), Cell(int64(InBounds.Max.Z) + InHaloStep)));
}

void FVoxelChangeHierarchy::SetNaturalInfluence(
	const int32 InHorizontalCells,
	const int32 InUpwardCells)
{
	FWriteScopeLock Scope(Lock);
	NaturalHorizontalInfluence = FMath::Max(0, InHorizontalCells);
	NaturalUpwardInfluence = FMath::Max(0, InUpwardCells);
}

FVoxelAffectedRepresentationKeys& FVoxelAffectedRepresentationKeys::operator=(FVoxelAffectedRepresentationKeys&& InOther)
{
	if (this != &InOther)
	{
		Reset();
		Storage = MoveTemp(InOther.Storage);
		FineShards = MoveTemp(InOther.FineShards);
		Proxy = MoveTemp(InOther.Proxy);
		Surface = MoveTemp(InOther.Surface);
		Macro = MoveTemp(InOther.Macro);
	}
	return *this;
}

void FVoxelAffectedRepresentationKeys::Reset()
{
	FineShards.Empty();
	Proxy.Empty();
	Surface.Empty();
	Macro.Empty();
	Storage.Release();
}

namespace
{
	bool SectionBounds(const FIntVector& InSection, FVoxelGenerationBounds& OutBounds)
	{
		const FInt64Vector Min(int64(InSection.X) * 16, int64(InSection.Y) * 16, int64(InSection.Z) * 16);
		return VoxelGeneration::ClipWorldBounds(Min, Min + FInt64Vector(16), OutBounds);
	}

	void VisitFineAffected(const FVoxelCoverageIndex& InIndex, const FVoxelGenerationBounds& InEdit,
		const int32 InHorizontal, const int32 InUpward, TFunctionRef<void(const FVoxelFineShardKey&)> InVisit)
	{
		const auto Cell = [](int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
		const int64 Horizontal = int64(InHorizontal) + 1;
		InIndex.AnyRecord(FIntVector(Cell(int64(InEdit.Min.X) - Horizontal), Cell(int64(InEdit.Min.Y) - Horizontal), Cell(int64(InEdit.Min.Z) - 1)),
			FIntVector(Cell(int64(InEdit.Max.X) + Horizontal), Cell(int64(InEdit.Max.Y) + Horizontal), Cell(int64(InEdit.Max.Z) + 1 + InUpward)),
			[&InVisit](const FVoxelCoverageRecord& Record)
			{
				InVisit({Record.Min, int32(Record.Geometry.Key.SideCells())});
				return false;
			});
	}

	void VisitGeometryAffected(const FVoxelCoverageIndex& InIndex, const FVoxelGenerationBounds& InEdit,
		const int32 InHorizontal, const int32 InUpward, const int32 InHalo,
		TFunctionRef<void(const FVoxelTerrainSpatialKey&)> InVisit)
	{
		const auto Cell = [](int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
		const int64 Horizontal = int64(InHalo) + InHorizontal;
		InIndex.AnyRecord(FIntVector(Cell(int64(InEdit.Min.X) - Horizontal), Cell(int64(InEdit.Min.Y) - Horizontal), Cell(int64(InEdit.Min.Z) - InHalo)),
			FIntVector(Cell(int64(InEdit.Max.X) + Horizontal), Cell(int64(InEdit.Max.Y) + Horizontal), Cell(int64(InEdit.Max.Z) + InHalo + InUpward)),
			[&](const FVoxelCoverageRecord& Record)
			{
				if (Record.Geometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy &&
					Record.Geometry.Key.Representation != EVoxelTerrainRepresentation::Surface &&
					Record.Geometry.Key.Representation != EVoxelTerrainRepresentation::Macro) return false;
				const int64 Step = int64(Record.Stamp.DependencyRevision);
				const int64 Margin = Step + InHorizontal;
				if (int64(Record.Min.X) - Margin < InEdit.Max.X && int64(Record.Max.X) + Margin > InEdit.Min.X &&
					int64(Record.Min.Y) - Margin < InEdit.Max.Y && int64(Record.Max.Y) + Margin > InEdit.Min.Y &&
					int64(Record.Min.Z) - Step - InUpward < InEdit.Max.Z && int64(Record.Max.Z) + Step > InEdit.Min.Z)
					InVisit(Record.Geometry.Key);
				return false;
			});
	}

	template<typename KeyType, typename ValueType>
	bool ReserveEntry(FVoxelChangeStorage& InStorage, TMap<KeyType, ValueType>& InMap,
		const KeyType& InKey, FString& OutError)
	{
		if (InMap.Contains(InKey)) return true;
		if (InMap.Num() == MAX_int32) return InStorage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
		return InStorage.Reserve(InMap, InMap.Num() + 1, OutError);
	}
}

void FVoxelChangeHierarchy::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	FWriteScopeLock Scope(Lock);
	Storage.BindResources(InResources, InConsumer);
	GeometryIndex.BindResources(InResources, InConsumer);
	SectionChangeIndex.BindResources(InResources, InConsumer);
	FineShardIndex.BindResources(InResources, InConsumer);
}

bool FVoxelChangeHierarchy::RegisterGeometry(const FVoxelTerrainGeometry& InGeometry, FString& OutError, bool* OutChanged)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	if (!InGeometry.Validate(OutError)) return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	const auto Previous = GeometryIndex.Find(InGeometry.Key);
	if (Previous && Previous->Geometry == InGeometry)
	{
		if (OutChanged) *OutChanged = false;
		Storage.Succeed();
		OutError.Reset();
		return true;
	}
	FVoxelCoverageRecord Record;
	Record.Geometry = InGeometry;
	if (!InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Record.Min, Record.Max))
		return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	const int32 OldStep = RetainedHaloSteps.FindRef(InGeometry.Key);
	const int32 RetainedStep = FMath::Max(OldStep, InGeometry.SampleStepCells);
	// 私有Geometry索引的记录同时冻结保留的依赖halo，供无锁快照过滤。
	Record.Stamp.DependencyRevision = uint64(RetainedStep);
	if (!ReserveEntry(Storage, RetainedHaloSteps, InGeometry.Key, OutError) ||
		!ReserveEntry(Storage, HaloStepCounts, RetainedStep, OutError)) return false;
	if (OldStep != RetainedStep && HaloStepCounts.FindRef(RetainedStep) == MAX_int32)
		return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	const FVoxelViewKey ProxyKey{InGeometry.Key.Coordinate, InGeometry.Key.Level};
	const FVoxelSurfaceTileKey SurfaceKey{FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
	const FVoxelMacroTileKey MacroKey{FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
	switch (InGeometry.Key.Representation)
	{
	case EVoxelTerrainRepresentation::VoxelProxy:
		if (!ReserveEntry(Storage, VoxelProxyRevision, ProxyKey, OutError)) return false;
		break;
	case EVoxelTerrainRepresentation::Surface:
		if (!ReserveEntry(Storage, SurfaceRevision, SurfaceKey, OutError)) return false;
		break;
	case EVoxelTerrainRepresentation::Macro:
		if (!ReserveEntry(Storage, MacroRevision, MacroKey, OutError)) return false;
		break;
	default:
		break;
	}
	FVoxelCoverageIndex Candidate = GeometryIndex;
	if (!PrepareCoverageRequirements(Candidate, OutError)) return false;
	if (!Candidate.Upsert(Record, OutError)) return Storage.Fail(Candidate.GetLastResourceAdmission(), OutError);
	const uint64 Revision = CaptureGeometryRevision(InGeometry, RetainedStep);
	GeometryIndex = MoveTemp(Candidate);
	GeometryIndex.MarkPublishedInputs();
	if (OldStep != RetainedStep)
	{
		if (OldStep > 0 && --HaloStepCounts.FindChecked(OldStep) == 0) HaloStepCounts.Remove(OldStep);
		++HaloStepCounts.FindOrAdd(RetainedStep);
		RetainedHaloSteps.FindOrAdd(InGeometry.Key) = RetainedStep;
		MaximumHaloStep = FMath::Max(MaximumHaloStep, RetainedStep);
	}
	switch (InGeometry.Key.Representation)
	{
	case EVoxelTerrainRepresentation::VoxelProxy:
		VoxelProxyRevision.FindOrAdd(ProxyKey) = FMath::Max(Revision, VoxelProxyRevision.FindRef(ProxyKey));
		break;
	case EVoxelTerrainRepresentation::Surface:
		SurfaceRevision.FindOrAdd(SurfaceKey) = FMath::Max(Revision, SurfaceRevision.FindRef(SurfaceKey));
		break;
	case EVoxelTerrainRepresentation::Macro:
		MacroRevision.FindOrAdd(MacroKey) = FMath::Max(Revision, MacroRevision.FindRef(MacroKey));
		break;
	default:
		break;
	}
	Storage.Succeed();
	if (OutChanged) *OutChanged = true;
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::ReleaseGeometry(const FVoxelTerrainSpatialKey& InKey, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	if (!PrepareCoverageRequirements(GeometryIndex, OutError)) return false;
	if (!GeometryIndex.Remove(InKey, OutError)) return Storage.Fail(GeometryIndex.GetLastResourceAdmission(), OutError);
	GeometryIndex.MarkPublishedInputs();
	const int32 Step = RetainedHaloSteps.FindRef(InKey);
	RetainedHaloSteps.Remove(InKey);
	if (Step > 0 && --HaloStepCounts.FindChecked(Step) == 0) HaloStepCounts.Remove(Step);
	if (Step == MaximumHaloStep && !HaloStepCounts.Contains(Step))
	{
		MaximumHaloStep = 1;
		for (const auto& Pair : HaloStepCounts) MaximumHaloStep = FMath::Max(MaximumHaloStep, Pair.Key);
	}
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::CanReleaseGeometryWithoutAllocation(const FVoxelTerrainSpatialKey& InKey) const
{
	FReadScopeLock Scope(Lock);
	return GeometryIndex.CanRemoveWithoutAllocation(InKey);
}

bool FVoxelChangeHierarchy::RegisterFineShard(const FVoxelFineShardKey& InKey, FString& OutError, bool* OutAdded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_FineIndexRegistration);
	if (!InKey.Validate(OutError) || (InKey.SideCells != 16 && InKey.SideCells != 32 && InKey.SideCells != 64))
		return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	const FVoxelTerrainSpatialKey Spatial{EVoxelTerrainRepresentation::Fine, InKey.Min / InKey.SideCells,
		uint8(FMath::FloorLog2(uint32(InKey.SideCells / 16)))};
	if (FineShardReferences.FindRef(InKey) == MAX_int32)
		return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	if (!ReserveEntry(Storage, FineShardReferences, InKey, OutError) ||
		!ReserveEntry(Storage, FineShardRevisions, InKey, OutError)) return false;
	if (FineShardIndex.Find(Spatial))
	{
		++FineShardReferences.FindChecked(InKey);
		if (OutAdded) *OutAdded = false;
		Storage.Succeed();
		OutError.Reset();
		return true;
	}
	FVoxelCoverageRecord Record;
	Record.Geometry.Key = Spatial;
	Record.Geometry.SampleStepCells = 1;
	Record.Min = InKey.Min;
	Record.Max = InKey.Bounds().Max;
	FVoxelCoverageIndex Candidate = FineShardIndex;
	if (!PrepareCoverageRequirements(Candidate, OutError)) return false;
	if (!Candidate.Upsert(Record, OutError)) return Storage.Fail(Candidate.GetLastResourceAdmission(), OutError);
	const uint64 Revision = CaptureGeometryRevision(Record.Geometry, 1);
	FineShardIndex = MoveTemp(Candidate);
	FineShardIndex.MarkPublishedInputs();
	FineShardReferences.Add(InKey, 1);
	FineShardRevisions.FindOrAdd(InKey) = FMath::Max(Revision, FineShardRevisions.FindRef(InKey));
	if (OutAdded) *OutAdded = true;
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::ReleaseFineShard(const FVoxelFineShardKey& InKey, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	int32* References = FineShardReferences.Find(InKey);
	if (References && *References == 1)
	{
		if (!PrepareCoverageRequirements(FineShardIndex, OutError)) return false;
		if (!FineShardIndex.Remove({EVoxelTerrainRepresentation::Fine, InKey.Min / InKey.SideCells,
			uint8(FMath::FloorLog2(uint32(InKey.SideCells / 16)))}, OutError))
			return Storage.Fail(FineShardIndex.GetLastResourceAdmission(), OutError);
		FineShardIndex.MarkPublishedInputs();
		FineShardReferences.Remove(InKey);
	}
	else if (References)
	{
		check(*References > 1);
		--*References;
	}
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::AffectsFineShard(const FVoxelFineShardKey& InKey, const FIntVector& InSection) const
{
	FVoxelGenerationBounds Edit;
	if (!SectionBounds(InSection, Edit)) return false;
	const auto Bounds = InKey.Bounds();
	const int64 Margin = int64(NaturalHorizontalInfluence) + 1;
	return int64(Bounds.Min.X) - Margin < Edit.Max.X && int64(Bounds.Max.X) + Margin > Edit.Min.X &&
		int64(Bounds.Min.Y) - Margin < Edit.Max.Y && int64(Bounds.Max.Y) + Margin > Edit.Min.Y &&
		int64(Bounds.Min.Z) - 1 - NaturalUpwardInfluence < Edit.Max.Z && int64(Bounds.Max.Z) + 1 > Edit.Min.Z;
}

bool FVoxelChangeHierarchy::GatherAffectedFineShards(const FIntVector& InSection,
	FVoxelAffectedRepresentationKeys& OutKeys, FString& OutError) const
{
	FReadScopeLock Scope(Lock);
	return GatherAffectedLocked(InSection, false, OutKeys, OutError);
}

bool FVoxelChangeHierarchy::GatherAffectedKeys(const FIntVector& InSection,
	FVoxelAffectedRepresentationKeys& OutKeys, FString& OutError) const
{
	FReadScopeLock Scope(Lock);
	return GatherAffectedLocked(InSection, true, OutKeys, OutError);
}

bool FVoxelChangeHierarchy::GatherAffectedLocked(const FIntVector& InSection, const bool bGeometry,
	FVoxelAffectedRepresentationKeys& OutKeys, FString& OutError) const
{
	FVoxelAffectedRepresentationKeys Candidate;
	if (Storage.GetResources()) Candidate.Storage.BindResources(Storage.GetResources().ToSharedRef(), Storage.GetConsumer());
	uint64 RequiredBytes = 0;
	int32 RequiredCount = 0;
	if (!MeasureNativeInput(RequiredBytes, RequiredCount, nullptr, nullptr, OutError)) return false;
	Candidate.Storage.SetRequirements(RequiredBytes, RequiredCount);
	FVoxelGenerationRequiredSourceScope PreviousOutput(Storage.GetResources().Get(), OutKeys.Storage.GetLease());
	FVoxelGenerationBounds Edit;
	const bool bValid = SectionBounds(InSection, Edit);
	int32 FineCount = 0;
	int32 ProxyCount = 0;
	int32 SurfaceCount = 0;
	int32 MacroCount = 0;
	if (bValid)
	{
		VisitFineAffected(FineShardIndex, Edit, NaturalHorizontalInfluence, NaturalUpwardInfluence,
			[&](const FVoxelFineShardKey&) { ++FineCount; });
		if (bGeometry) VisitGeometryAffected(GeometryIndex, Edit, NaturalHorizontalInfluence, NaturalUpwardInfluence, MaximumHaloStep,
			[&](const FVoxelTerrainSpatialKey& Key)
			{
				switch (Key.Representation)
				{
				case EVoxelTerrainRepresentation::VoxelProxy: ++ProxyCount; break;
				case EVoxelTerrainRepresentation::Surface: ++SurfaceCount; break;
				case EVoxelTerrainRepresentation::Macro: ++MacroCount; break;
				default: break;
				}
			});
	}
	if (!Candidate.Storage.Reserve(Candidate.FineShards, FineCount, OutError) ||
		!Candidate.Storage.Reserve(Candidate.Proxy, ProxyCount, OutError) ||
		!Candidate.Storage.Reserve(Candidate.Surface, SurfaceCount, OutError) ||
		!Candidate.Storage.Reserve(Candidate.Macro, MacroCount, OutError))
		return Storage.Fail(Candidate.Storage.GetAdmission(), OutError);
	if (bValid)
	{
		VisitFineAffected(FineShardIndex, Edit, NaturalHorizontalInfluence, NaturalUpwardInfluence,
			[&](const FVoxelFineShardKey& Key) { Candidate.FineShards.Add(Key); });
		if (bGeometry) VisitGeometryAffected(GeometryIndex, Edit, NaturalHorizontalInfluence, NaturalUpwardInfluence, MaximumHaloStep,
			[&](const FVoxelTerrainSpatialKey& Key)
			{
				switch (Key.Representation)
				{
				case EVoxelTerrainRepresentation::VoxelProxy: Candidate.Proxy.Add({Key.Coordinate, Key.Level}); break;
				case EVoxelTerrainRepresentation::Surface: Candidate.Surface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}); break;
				case EVoxelTerrainRepresentation::Macro: Candidate.Macro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}); break;
				default: break;
				}
			});
	}
	OutKeys = MoveTemp(Candidate);
	Storage.Succeed();
	OutError.Reset();
	return true;
}

uint64 FVoxelChangeHierarchy::GetFineShardRevision(const FVoxelFineShardKey& InKey) const
{
	FReadScopeLock Scope(Lock);
	return FineShardRevisions.FindRef(InKey);
}

void FVoxelChangeHierarchy::SetTopology(const FVoxelLodTopologySettings& InLod)
{
	FWriteScopeLock Scope(Lock);
	FString Error;
	checkf(VoxelViewLod::ValidateTopology(InLod, Error), TEXT("%s"), *Error);
}

bool FVoxelChangeHierarchy::PrepareInvalidations(const int32 InCount, TFunctionRef<FIntVector(int32)> InSection,
	FVoxelPreparedChangeInvalidations& OutPrepared, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	if (InCount < 0) return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	if (uint64(InCount) > MAX_uint64 - RevisionSerial)
		return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	int64 Count = SectionRevision.Num();
	for (int32 Index = 0; Index < InCount; ++Index)
	{
		FVoxelGenerationBounds Bounds;
		if (!SectionBounds(InSection(Index), Bounds)) return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
		if (SectionRevision.Contains(InSection(Index))) continue;
		bool bPrevious = false;
		for (int32 Previous = 0; Previous < Index; ++Previous)
			if (InSection(Previous) == InSection(Index)) { bPrevious = true; break; }
		if (!bPrevious) ++Count;
	}
	if (Count > MAX_int32) return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	if (!Storage.Reserve(SectionRevision, int32(Count), OutError)) return false;
	FVoxelPreparedChangeInvalidations Candidate{SectionChangeIndex, this, RevisionSerial, InCount};
	for (int32 Index = 0; Index < InCount; ++Index)
	{
		FVoxelCoverageRecord Edit;
		Edit.Geometry.Key = {EVoxelTerrainRepresentation::Fine, InSection(Index), 0};
		Edit.Geometry.SampleStepCells = 1;
		Edit.Stamp.ContentRevision = RevisionSerial + uint64(Index) + 1;
		FVoxelGenerationBounds Bounds;
		if (!SectionBounds(InSection(Index), Bounds)) return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
		Edit.Min = Bounds.Min;
		Edit.Max = Bounds.Max;
		if (!PrepareCoverageRequirements(Candidate.SectionChanges, OutError, &OutPrepared.SectionChanges)) return false;
		if (!Candidate.SectionChanges.Upsert(Edit, OutError))
			return Storage.Fail(Candidate.SectionChanges.GetLastResourceAdmission(), OutError);
	}
	OutPrepared = MoveTemp(Candidate);
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::CommitInvalidations(FVoxelPreparedChangeInvalidations&& InPrepared,
	TFunctionRef<FIntVector(int32)> InSection, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (InPrepared.Origin != this || InPrepared.Count < 0 || uint64(InPrepared.Count) > MAX_uint64 - RevisionSerial ||
		InPrepared.BeforeRevision != RevisionSerial)
	{
		OutError = TEXT("Stale: change hierarchy preparation cut changed");
		return false;
	}
	for (int32 Index = 0; Index < InPrepared.Count; ++Index)
	{
		const auto Record = InPrepared.SectionChanges.Find({EVoxelTerrainRepresentation::Fine, InSection(Index), 0});
		// 重复Section以最后一次出现的不可变记录版本为准。
		int32 Last = Index;
		for (int32 Later = Index + 1; Later < InPrepared.Count; ++Later)
			if (InSection(Later) == InSection(Index)) Last = Later;
		if (!Record || Record->Stamp.ContentRevision != RevisionSerial + uint64(Last) + 1)
			return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	}
	SectionChangeIndex = MoveTemp(InPrepared.SectionChanges);
	SectionChangeIndex.MarkPublishedInputs();
	for (int32 Index = 0; Index < InPrepared.Count; ++Index)
	{
		const FIntVector Section = InSection(Index);
		const uint64 Revision = ++RevisionSerial;
		SectionRevision.FindOrAdd(Section) = Revision;
		FVoxelGenerationBounds Edit;
		const bool bHasBounds = SectionBounds(Section, Edit);
		check(bHasBounds);
		VisitFineAffected(FineShardIndex, Edit, NaturalHorizontalInfluence, NaturalUpwardInfluence,
			[&](const FVoxelFineShardKey& Key) { FineShardRevisions.FindChecked(Key) = Revision; });
		VisitGeometryAffected(GeometryIndex, Edit, NaturalHorizontalInfluence, NaturalUpwardInfluence, MaximumHaloStep,
			[&](const FVoxelTerrainSpatialKey& Key)
			{
				switch (Key.Representation)
				{
				case EVoxelTerrainRepresentation::VoxelProxy: VoxelProxyRevision.FindChecked({Key.Coordinate, Key.Level}) = Revision; break;
				case EVoxelTerrainRepresentation::Surface: SurfaceRevision.FindChecked({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}) = Revision; break;
				case EVoxelTerrainRepresentation::Macro: MacroRevision.FindChecked({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}) = Revision; break;
				default: break;
				}
			});
	}
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::InvalidateSection(const FIntVector& InSection, uint64& OutRevision, FString& OutError)
{
	FVoxelPreparedChangeInvalidations Prepared;
	const auto Section = [&InSection](int32) { return InSection; };
	if (!PrepareInvalidations(1, Section, Prepared, OutError) || !CommitInvalidations(MoveTemp(Prepared), Section, OutError)) return false;
	OutRevision = GetContentRevision();
	return true;
}

bool FVoxelChangeHierarchy::AffectsGeometry(const FVoxelTerrainSpatialKey& InKey, const FIntVector& InSection) const
{
	const auto Record = GeometryIndex.Find(InKey);
	if (!Record) return false;
	const int64 Step = RetainedHaloSteps.FindChecked(InKey);
	const int64 Margin = Step + NaturalHorizontalInfluence;
	FVoxelGenerationBounds Edit;
	if (!SectionBounds(InSection, Edit)) return false;
	const FIntVector EditMin = Edit.Min;
	const FIntVector EditMax = Edit.Max;
	return int64(Record->Min.X) - Margin < EditMax.X && int64(Record->Max.X) + Margin > EditMin.X &&
		int64(Record->Min.Y) - Margin < EditMax.Y && int64(Record->Max.Y) + Margin > EditMin.Y &&
		int64(Record->Min.Z) - Step - NaturalUpwardInfluence < EditMax.Z && int64(Record->Max.Z) + Step > EditMin.Z;
}

uint64 FVoxelChangeHierarchy::CaptureGeometryRevision(const FVoxelTerrainGeometry& InGeometry, const int32 InRetainedHaloStep) const
{
	FIntVector Min, Max;
	check(InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Min, Max));
	const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
	const int64 Horizontal = int64(InRetainedHaloStep) + NaturalHorizontalInfluence;
	uint64 Revision = 0;
	SectionChangeIndex.AnyRecord(FIntVector(Cell(int64(Min.X) - Horizontal), Cell(int64(Min.Y) - Horizontal),
		Cell(int64(Min.Z) - InRetainedHaloStep - NaturalUpwardInfluence)),
		FIntVector(Cell(int64(Max.X) + Horizontal), Cell(int64(Max.Y) + Horizontal), Cell(int64(Max.Z) + InRetainedHaloStep)),
		[&Revision](const FVoxelCoverageRecord& Record)
		{
			Revision = FMath::Max(Revision, Record.Stamp.ContentRevision);
			return false;
		});
	return Revision;
}


uint64 FVoxelChangeHierarchy::GetSectionRevision(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = SectionRevision.Find(InSection);
	return Found ? *Found : 0;
}

uint64 FVoxelChangeHierarchy::GetContentRevision() const
{
	FReadScopeLock Scope(Lock);
	return RevisionSerial;
}

uint64 FVoxelChangeHierarchy::GetVoxelProxyRevision(const FVoxelViewKey& InProxyKey) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = VoxelProxyRevision.Find(InProxyKey);
	return Found ? *Found : 0;
}

uint64 FVoxelChangeHierarchy::GetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = SurfaceRevision.Find(InSurfaceTile);
	return Found ? *Found : 0;
}

uint64 FVoxelChangeHierarchy::GetMacroRevision(const FVoxelMacroTileKey& InMacroTile) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = MacroRevision.Find(InMacroTile);
	return Found ? *Found : 0;
}

bool FVoxelChangeHierarchy::SetVoxelProxyRevision(const FVoxelViewKey& InKey, const uint64 InRevision, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	if (!ReserveEntry(Storage, VoxelProxyRevision, InKey, OutError)) return false;
	VoxelProxyRevision.FindOrAdd(InKey) = InRevision;
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::SetSurfaceRevision(const FVoxelSurfaceTileKey& InKey, const uint64 InRevision, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	if (!ReserveEntry(Storage, SurfaceRevision, InKey, OutError)) return false;
	SurfaceRevision.FindOrAdd(InKey) = InRevision;
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::SetMacroRevision(const FVoxelMacroTileKey& InKey, const uint64 InRevision, FString& OutError)
{
	FWriteScopeLock Scope(Lock);
	if (!PrepareStorageRequirements(OutError)) return false;
	if (!ReserveEntry(Storage, MacroRevision, InKey, OutError)) return false;
	MacroRevision.FindOrAdd(InKey) = InRevision;
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
	Storage.Succeed();
	OutError.Reset();
	return true;
}

bool FVoxelChangeHierarchy::AffectsVoxelProxy(const FVoxelViewKey& InKey, const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	return AffectsGeometry({EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level}, InSection);
}

bool FVoxelChangeHierarchy::AffectsSurface(const FVoxelSurfaceTileKey& InKey, const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	return AffectsGeometry({EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}, InSection);
}

bool FVoxelChangeHierarchy::AffectsMacro(const FVoxelMacroTileKey& InKey, const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	return AffectsGeometry({EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level}, InSection);
}

void FVoxelChangeHierarchy::ReleaseSection(const FIntVector& InSection)
{
	FWriteScopeLock Scope(Lock);
	// 权威内容版本属于世界事实，Runtime 驱逐不释放它。
	(void)InSection;
}


void FVoxelChangeHierarchy::VisitAffectedFineShards(const FIntVector& InSection,
	TFunctionRef<void(const FVoxelFineShardKey&)> InVisit) const
{
	FVoxelCoverageIndex Frozen;
	int32 HorizontalInfluence;
	int32 UpwardInfluence;
	{
		FReadScopeLock Scope(Lock);
		Frozen = FineShardIndex;
		HorizontalInfluence = NaturalHorizontalInfluence;
		UpwardInfluence = NaturalUpwardInfluence;
	}
	FVoxelGenerationBounds Edit;
	if (!SectionBounds(InSection, Edit)) return;
	VisitFineAffected(Frozen, Edit, HorizontalInfluence, UpwardInfluence, InVisit);
}

void FVoxelChangeHierarchy::VisitAffectedKeys(const FIntVector& InSection,
	TFunctionRef<void(const FVoxelTerrainSpatialKey&)> InVisit) const
{
	FVoxelCoverageIndex Frozen;
	int32 HorizontalInfluence;
	int32 UpwardInfluence;
	int32 Halo;
	{
		FReadScopeLock Scope(Lock);
		Frozen = GeometryIndex;
		HorizontalInfluence = NaturalHorizontalInfluence;
		UpwardInfluence = NaturalUpwardInfluence;
		Halo = MaximumHaloStep;
	}
	FVoxelGenerationBounds Edit;
	if (!SectionBounds(InSection, Edit)) return;
	VisitGeometryAffected(Frozen, Edit, HorizontalInfluence, UpwardInfluence, Halo, InVisit);
}

namespace
{
	template<typename ObjectType>
	struct TChangeSharedDeleter
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		void operator()(ObjectType* Object) const
		{
			delete Object;
		}
	};

	template<typename ObjectType, typename DeleterType>
	uint64 ChangeSharedBytes()
	{
		using FController = SharedPointerInternals::TReferenceControllerWithDeleter<ObjectType, DeleterType, ESPMode::ThreadSafe>;
		return FMemory::QuantizeSize(sizeof(ObjectType), alignof(ObjectType)) +
			FMemory::QuantizeSize(sizeof(FController), alignof(FController));
	}
}

struct FVoxelChangeHierarchy::FFineReleaseNode
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ControlLease;
	FVoxelFineShardKey Key;
	uint64 NativeInputBytes = 0;
	int32 NativeLeaseCount = 0;
	FFineReleaseNode* Next = nullptr;
	uint64 ReleaseVersion = 0;
	bool bWaiting = false;
	bool bPermanentFailure = false;
};

struct FVoxelChangeHierarchy::FFineReleaseState
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FCriticalSection Mutex;
	FFineReleaseNode* Head = nullptr;
	FFineReleaseNode* Tail = nullptr;
	uint64 NativeNodeInputBytes = 0;
	int32 NativeNodeLeaseCount = 0;
	bool bDetached = false;
};

struct FVoxelChangeHierarchy::FFineNodeDeleter
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FFineReleaseState, ESPMode::ThreadSafe> State;
	void operator()(FFineReleaseNode* Node) const
	{
		bool bDelete;
		{
			FScopeLock Scope(&State->Mutex);
			bDelete = State->bDetached;
			if (bDelete)
			{
				State->NativeNodeInputBytes -= Node->NativeInputBytes;
				State->NativeNodeLeaseCount -= Node->NativeLeaseCount;
			}
			if (!bDelete)
			{
				if (State->Tail) State->Tail->Next = Node;
				else State->Head = Node;
				State->Tail = Node;
			}
		}
		if (bDelete) delete Node;
	}
};

bool FVoxelChangeHierarchy::MeasureNativeInput(uint64& OutBytes, int32& OutCount,
	const FVoxelCoverageIndex* InCandidate, const FVoxelCoverageIndex* InRetainedOutput, FString& OutError) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_FineNativeInputMeasure);
	const auto Resources = Storage.GetResources();
	uint64 Bytes = Storage.GetBytes();
	int32 Count = Storage.GetLeaseCount();
	if (Resources)
	{
		const FVoxelCoverageIndex* Roots[] = {&GeometryIndex, &SectionChangeIndex, &FineShardIndex, InCandidate, InRetainedOutput};
		if (!FVoxelCoverageIndex::AccumulateRetainedInputs(*Resources,
			MakeArrayView(Roots, InRetainedOutput ? 5 : InCandidate ? 4 : 3), {}, Bytes, Count))
			return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
		if (FineReleaseState && FineReleaseState->Lease)
		{
			FScopeLock StateScope(&FineReleaseState->Mutex);
			const uint64 RootBytes = FineReleaseState->Lease->Bytes();
			if (RootBytes > MAX_uint64 - FineReleaseState->NativeNodeInputBytes ||
				RootBytes + FineReleaseState->NativeNodeInputBytes > MAX_uint64 - Bytes ||
				FineReleaseState->NativeNodeLeaseCount >= MAX_int32 - Count)
				return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
			Bytes += RootBytes + FineReleaseState->NativeNodeInputBytes;
			Count += 1 + FineReleaseState->NativeNodeLeaseCount;
		}
	}
	OutBytes = Bytes;
	OutCount = Count;
	return true;
}

bool FVoxelChangeHierarchy::PrepareStorageRequirements(FString& OutError) const
{
	uint64 Bytes = 0;
	int32 Count = 0;
	if (!MeasureNativeInput(Bytes, Count, nullptr, nullptr, OutError)) return false;
	Storage.SetRequirements(Bytes - Storage.GetBytes(), Count - Storage.GetLeaseCount());
	return true;
}

bool FVoxelChangeHierarchy::PrepareCoverageRequirements(FVoxelCoverageIndex& InCandidate,
	FString& OutError, const FVoxelCoverageIndex* InRetainedOutput) const
{
	const auto Resources = Storage.GetResources();
	if (!Resources) return true;
	uint64 AllBytes = 0, OwnBytes = 0;
	int32 AllCount = 0, OwnCount = 0;
	if (!MeasureNativeInput(AllBytes, AllCount, &InCandidate, InRetainedOutput, OutError) ||
		!InCandidate.AccumulateRetainedInput(*Resources, nullptr, {}, OwnBytes, OwnCount))
		return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	FVoxelGenerationRequiredCapacity Sources;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, Storage.GetLease(), Sources, OutError) ||
		Sources.Bytes[int32(EVoxelTerrainResourcePool::Input)] > MAX_uint64 - AllBytes || Sources.LeaseCount > MAX_int32 - AllCount)
		return Storage.Fail(EVoxelTerrainResourceAdmission::CapacityUnachievable, OutError);
	AllBytes += Sources.Bytes[int32(EVoxelTerrainResourcePool::Input)];
	AllCount += Sources.LeaseCount;
	check(OwnBytes <= AllBytes && OwnCount <= AllCount);
	InCandidate.SetAdditionalRequiredInput(AllBytes - OwnBytes, AllCount - OwnCount);
	return true;
}

bool FVoxelChangeHierarchy::PinFineShard(const FVoxelFineShardKey& InKey,
	TSharedPtr<const void, ESPMode::ThreadSafe>& OutToken, FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_FinePinRegistration);
	check(IsInGameThread());
	if (!InKey.Validate(OutError)) return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	const auto Resources = Storage.GetResources();
	const auto Consumer = Storage.GetConsumer();
	if (Resources && Resources->GetControlConsumer().IsValid() && Resources->GetControlConsumer().WorldEpoch != Consumer.WorldEpoch)
		return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
	uint64 RequiredBytes = 0;
	int32 RequiredCount = 0;
	using FStateDeleter = TChangeSharedDeleter<FFineReleaseState>;
	if (!FineReleaseState)
	{
		if (!MeasureNativeInput(RequiredBytes, RequiredCount, nullptr, nullptr, OutError)) return false;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (Resources)
		{
			const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
				ChangeSharedBytes<FFineReleaseState, FStateDeleter>(), Lease, 0, RequiredBytes, RequiredCount);
			if (Admission != EVoxelTerrainResourceAdmission::Granted) return Storage.Fail(Admission, OutError);
		}
#if !WITH_DEV_AUTOMATION_TESTS
		else return Storage.Fail(EVoxelTerrainResourceAdmission::InvalidRequest, OutError);
#endif
		FineReleaseState = TSharedPtr<FFineReleaseState, ESPMode::ThreadSafe>(new FFineReleaseState, FStateDeleter{Lease});
		FineReleaseState->Lease = MoveTemp(Lease);
	}
	if (!MeasureNativeInput(RequiredBytes, RequiredCount, nullptr, nullptr, OutError)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (Resources)
	{
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			ChangeSharedBytes<FFineReleaseNode, FFineNodeDeleter>(), Lease, 0, RequiredBytes, RequiredCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted) return Storage.Fail(Admission, OutError);
	}
	FVoxelGenerationRequiredSourceScope NodeSource(Resources.Get(), Lease.Get());
	if (!RegisterFineShard(InKey, OutError)) return false;
	FFineReleaseNode* Node = new FFineReleaseNode;
	Node->Key = InKey;
	Node->Lease = Lease;
	Node->ControlLease = FineReleaseState->Lease;
	Node->NativeInputBytes = Lease ? Lease->Bytes() : 0;
	Node->NativeLeaseCount = int32(Lease.IsValid());
	{
		FScopeLock StateScope(&FineReleaseState->Mutex);
		FineReleaseState->NativeNodeInputBytes += Node->NativeInputBytes;
		FineReleaseState->NativeNodeLeaseCount += Node->NativeLeaseCount;
	}
	TSharedPtr<FFineReleaseNode, ESPMode::ThreadSafe> Token(Node, FFineNodeDeleter{MoveTemp(Lease), FineReleaseState});
	OutToken = TSharedPtr<const void, ESPMode::ThreadSafe>(Token, static_cast<const void*>(Node));
	OutError.Reset();
	return true;
}

void FVoxelChangeHierarchy::VisitFineRegistrationSources(const TSharedPtr<const void, ESPMode::ThreadSafe>& InToken,
	const FVoxelTerrainResources& InResources, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
{
	if (!InToken) return;
	const auto* Node = static_cast<const FFineReleaseNode*>(InToken.Get());
	for (const auto* Lease : {Node->Lease.Get(), Node->ControlLease.Get()})
	{
		if (Lease && Lease->IsOwnedBy(InResources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, InVisit);
	}
}

int32 FVoxelChangeHierarchy::AdvanceFineReleases(const int32 InMaximum)
{
	check(IsInGameThread());
	const auto State = FineReleaseState;
	if (!State || InMaximum <= 0) return 0;
	int32 Processed = 0;
	for (; Processed < InMaximum; ++Processed)
	{
		FFineReleaseNode* Node;
		{
			FScopeLock Scope(&State->Mutex);
			Node = State->Head;
		}
		if (!Node || Node->bPermanentFailure) break;
		const auto Resources = Storage.GetResources();
		const uint64 Before = Resources ? Resources->ReleaseVersionExcludingOwner(Storage.GetConsumer()) : 0;
		if (Node->bWaiting && Before == Node->ReleaseVersion)
		{
			FReadScopeLock Scope(Lock);
			const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Fine, Node->Key.Min / Node->Key.SideCells,
				uint8(FMath::FloorLog2(uint32(Node->Key.SideCells / 16)))};
			if (!FineShardIndex.CanRemoveWithoutAllocation(Key)) break;
		}
		FString Error;
		if (!ReleaseFineShard(Node->Key, Error))
		{
			Node->ReleaseVersion = Before;
			Node->bWaiting = GetLastResourceAdmission() == EVoxelTerrainResourceAdmission::CapacityBlocked;
			Node->bPermanentFailure = !Node->bWaiting;
			break;
		}
		{
			FScopeLock Scope(&State->Mutex);
			check(State->Head == Node);
			State->Head = Node->Next;
			State->NativeNodeInputBytes -= Node->NativeInputBytes;
			State->NativeNodeLeaseCount -= Node->NativeLeaseCount;
			if (!State->Head) State->Tail = nullptr;
		}
		// 即使controller已退休，节点授额也必须保留到原生delete结束。
		auto RetiringLease = MoveTemp(Node->Lease);
		delete Node;
		RetiringLease.Reset();
	}
	return Processed;
}

bool FVoxelChangeHierarchy::HasPendingFineReleases() const
{
	const auto State = FineReleaseState;
	if (!State) return false;
	FScopeLock Scope(&State->Mutex);
	return State->Head != nullptr;
}

FVoxelChangeHierarchy::~FVoxelChangeHierarchy()
{
	Reset();
}

void FVoxelChangeHierarchy::Reset()
{
	FWriteScopeLock Scope(Lock);
	if (FineReleaseState)
	{
		FFineReleaseNode* Pending;
		{
			FScopeLock StateScope(&FineReleaseState->Mutex);
			FineReleaseState->bDetached = true;
			Pending = FineReleaseState->Head;
			for (auto* Node = Pending; Node; Node = Node->Next)
			{
				FineReleaseState->NativeNodeInputBytes -= Node->NativeInputBytes;
				FineReleaseState->NativeNodeLeaseCount -= Node->NativeLeaseCount;
			}
			FineReleaseState->Head = FineReleaseState->Tail = nullptr;
		}
		while (Pending)
		{
			FFineReleaseNode* Next = Pending->Next;
			auto RetiringLease = MoveTemp(Pending->Lease);
			delete Pending;
			RetiringLease.Reset();
			Pending = Next;
		}
		FineReleaseState.Reset();
	}
	GeometryIndex.Reset();
	SectionChangeIndex.Reset();
	FineShardIndex.Reset();
	FineShardRevisions.Empty();
	FineShardReferences.Empty();
	RetainedHaloSteps.Empty();
	HaloStepCounts.Empty();
	MaximumHaloStep = 1;
	RevisionSerial = 0;
	SectionRevision.Empty();
	VoxelProxyRevision.Empty();
	SurfaceRevision.Empty();
	MacroRevision.Empty();
	Storage.Release();
}

#if WITH_DEV_AUTOMATION_TESTS
bool FVoxelChangeHierarchy::RegisterGeometry(const FVoxelTerrainGeometry& InGeometry)
{
	FString Error;
	bool bChanged = false;
	verify(RegisterGeometry(InGeometry, Error, &bChanged));
	return bChanged;
}

bool FVoxelChangeHierarchy::RegisterFineShard(const FVoxelFineShardKey& InKey)
{
	FString Error;
	bool bAdded = false;
	verify(RegisterFineShard(InKey, Error, &bAdded));
	return bAdded;
}

void FVoxelChangeHierarchy::ReleaseFineShard(const FVoxelFineShardKey& InKey)
{
	FString Error;
	verify(ReleaseFineShard(InKey, Error));
}

void FVoxelChangeHierarchy::ReleaseGeometry(const FVoxelTerrainSpatialKey& InKey)
{
	FString Error;
	verify(ReleaseGeometry(InKey, Error));
}

TSet<FVoxelFineShardKey> FVoxelChangeHierarchy::GatherAffectedFineShards(const FIntVector& InSection) const
{
	FVoxelAffectedRepresentationKeys Keys;
	FString Error;
	check(GatherAffectedFineShards(InSection, Keys, Error));
	check(!Keys.Storage.GetResources());
	return MoveTemp(Keys.FineShards);
}

FVoxelAffectedRepresentationKeys FVoxelChangeHierarchy::GatherAffectedKeys(const FIntVector& InSection) const
{
	FVoxelAffectedRepresentationKeys Keys;
	FString Error;
	check(GatherAffectedKeys(InSection, Keys, Error));
	return Keys;
}

uint64 FVoxelChangeHierarchy::InvalidateSection(const FIntVector& InSection)
{
	FString Error;
	uint64 Revision = 0;
	check(InvalidateSection(InSection, Revision, Error));
	return Revision;
}

void FVoxelChangeHierarchy::SetVoxelProxyRevision(const FVoxelViewKey& InKey, const uint64 InRevision)
{
	FString Error;
	check(SetVoxelProxyRevision(InKey, InRevision, Error));
}

void FVoxelChangeHierarchy::SetSurfaceRevision(const FVoxelSurfaceTileKey& InKey, const uint64 InRevision)
{
	FString Error;
	check(SetSurfaceRevision(InKey, InRevision, Error));
}

void FVoxelChangeHierarchy::SetMacroRevision(const FVoxelMacroTileKey& InKey, const uint64 InRevision)
{
	FString Error;
	check(SetMacroRevision(InKey, InRevision, Error));
}
#endif
