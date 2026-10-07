#include "Voxel/Runtime/VoxelChangeHierarchy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"

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

bool FVoxelChangeHierarchy::RegisterGeometry(const FVoxelTerrainGeometry& InGeometry)
{
	FWriteScopeLock Scope(Lock);
	const auto Previous = GeometryIndex.Find(InGeometry.Key);
	if (Previous && Previous->Geometry == InGeometry) return false;
	auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Record->Geometry = InGeometry;
	FString Error;
	checkf(InGeometry.Validate(Error), TEXT("%s"), *Error);
	check(InGeometry.Key.TryBounds(InGeometry.WorldMinZ, InGeometry.WorldMaxZ, Record->Min, Record->Max));
	// The spatial index retains exact key bounds. Dependency halos are query/filter metadata.
	const int32 OldStep = RetainedHaloSteps.FindRef(InGeometry.Key);
	const int32 RetainedStep = FMath::Max(OldStep, InGeometry.SampleStepCells);
	if (OldStep != RetainedStep)
	{
		if (OldStep > 0 && --HaloStepCounts.FindChecked(OldStep) == 0) HaloStepCounts.Remove(OldStep);
		++HaloStepCounts.FindOrAdd(RetainedStep);
		RetainedHaloSteps.Add(InGeometry.Key, RetainedStep);
		MaximumHaloStep = FMath::Max(MaximumHaloStep, RetainedStep);
	}
	checkf(GeometryIndex.Upsert(Record, Error), TEXT("%s"), *Error);
	// 新注册表示也必须继承非驻留时期的编辑，而非从零内容版本开始。
	const uint64 Revision = CaptureGeometryRevision(InGeometry, RetainedStep);
	switch (InGeometry.Key.Representation)
	{
	case EVoxelTerrainRepresentation::VoxelProxy:
	{
		const FVoxelViewKey Key{InGeometry.Key.Coordinate, InGeometry.Key.Level};
		VoxelProxyRevision.Add(Key, FMath::Max(Revision, VoxelProxyRevision.FindRef(Key)));
		break;
	}
	case EVoxelTerrainRepresentation::Surface:
	{
		const FVoxelSurfaceTileKey Key{FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
		SurfaceRevision.Add(Key, FMath::Max(Revision, SurfaceRevision.FindRef(Key)));
		break;
	}
	case EVoxelTerrainRepresentation::Macro:
	{
		const FVoxelMacroTileKey Key{FIntPoint(InGeometry.Key.Coordinate.X, InGeometry.Key.Coordinate.Y), InGeometry.Key.Level};
		MacroRevision.Add(Key, FMath::Max(Revision, MacroRevision.FindRef(Key)));
		break;
	}
	default:
		break;
	}
	return true;
}

void FVoxelChangeHierarchy::ReleaseGeometry(const FVoxelTerrainSpatialKey& InKey)
{
	FWriteScopeLock Scope(Lock);
	GeometryIndex.Remove(InKey);
	const int32 Step = RetainedHaloSteps.FindRef(InKey);
	RetainedHaloSteps.Remove(InKey);
	if (Step > 0 && --HaloStepCounts.FindChecked(Step) == 0) HaloStepCounts.Remove(Step);
	if (Step == MaximumHaloStep && !HaloStepCounts.Contains(Step))
	{
		MaximumHaloStep = 1;
		for (const auto& Pair : HaloStepCounts) MaximumHaloStep = FMath::Max(MaximumHaloStep, Pair.Key);
	}
}

bool FVoxelChangeHierarchy::RegisterFineShard(const FVoxelFineShardKey& InKey)
{
	FString Error;
	checkf(InKey.Validate(Error), TEXT("%s"), *Error);
	check(InKey.SideCells == 16 || InKey.SideCells == 32 || InKey.SideCells == 64);
	FWriteScopeLock Scope(Lock);
	const FVoxelTerrainSpatialKey Spatial{EVoxelTerrainRepresentation::Fine, InKey.Min / InKey.SideCells,
		uint8(FMath::FloorLog2(uint32(InKey.SideCells / 16)))};
	check(FineShardReferences.FindRef(InKey) < MAX_int32);
	++FineShardReferences.FindOrAdd(InKey);
	if (FineShardIndex.Find(Spatial)) return false;
	auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Record->Geometry.Key = Spatial;
	Record->Geometry.SampleStepCells = 1;
	Record->Min = InKey.Min;
	Record->Max = InKey.Bounds().Max;
	const uint64 Revision = CaptureGeometryRevision(Record->Geometry, 1);
	FineShardRevisions.Add(InKey, FMath::Max(Revision, FineShardRevisions.FindRef(InKey)));
	checkf(FineShardIndex.Upsert(Record, Error), TEXT("%s"), *Error);
	return true;
}

void FVoxelChangeHierarchy::ReleaseFineShard(const FVoxelFineShardKey& InKey)
{
	FWriteScopeLock Scope(Lock);
	int32* References = FineShardReferences.Find(InKey);
	if (!References) return;
	check(*References > 0);
	if (--*References > 0) return;
	FineShardReferences.Remove(InKey);
	FineShardIndex.Remove({EVoxelTerrainRepresentation::Fine, InKey.Min / InKey.SideCells,
		uint8(FMath::FloorLog2(uint32(InKey.SideCells / 16)))});
}

TSet<FVoxelFineShardKey> FVoxelChangeHierarchy::GatherAffectedFineShards(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	return GatherAffectedFineShardsLocked(InSection);
}

TSet<FVoxelFineShardKey> FVoxelChangeHierarchy::GatherAffectedFineShardsLocked(const FIntVector& InSection) const
{
	TSet<FVoxelFineShardKey> Keys;
	const FIntVector Min = InSection * 16;
	const FIntVector Max = Min + FIntVector(16);
	const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
	const int64 Horizontal = int64(NaturalHorizontalInfluence) + 1;
	FVoxelCoverageLocalSnapshot Local;
	FineShardIndex.Capture(FIntVector(Cell(int64(Min.X) - Horizontal), Cell(int64(Min.Y) - Horizontal), Cell(int64(Min.Z) - 1)),
		FIntVector(Cell(int64(Max.X) + Horizontal), Cell(int64(Max.Y) + Horizontal), Cell(int64(Max.Z) + 1 + NaturalUpwardInfluence)), Local);
	for (const auto& Record : Local.Records) Keys.Add({Record->Min, int32(Record->Geometry.Key.SideCells())});
	return Keys;
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

uint64 FVoxelChangeHierarchy::InvalidateSection(const FIntVector& InSection)
{
	const FVoxelAffectedRepresentationKeys Keys = GatherAffectedKeys(InSection);
	FWriteScopeLock Scope(Lock);
	check(RevisionSerial != MAX_uint64);
	const uint64 Revision = ++RevisionSerial;
	SectionRevision.Add(InSection, Revision);
	auto Edit = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Edit->Geometry.Key = {EVoxelTerrainRepresentation::Fine, InSection, 0};
	Edit->Geometry.SampleStepCells = 1;
	Edit->Stamp.ContentRevision = Revision;
	check(Edit->Geometry.Key.TryBounds(0, 16, Edit->Min, Edit->Max));
	FString Error;
	checkf(SectionChangeIndex.Upsert(Edit, Error), TEXT("%s"), *Error);
	for (const auto& Key : Keys.Proxy) VoxelProxyRevision.Add(Key, Revision);
	for (const auto& Key : Keys.Surface) SurfaceRevision.Add(Key, Revision);
	for (const auto& Key : Keys.Macro) MacroRevision.Add(Key, Revision);
	for (const auto& Key : Keys.FineShards) FineShardRevisions.Add(Key, Revision);
	return Revision;
}

FVoxelAffectedRepresentationKeys FVoxelChangeHierarchy::GatherAffectedKeys(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	FVoxelAffectedRepresentationKeys Result;
	Result.FineShards = GatherAffectedFineShardsLocked(InSection);
	const FIntVector EditMin = InSection * 16;
	const FIntVector EditMax = EditMin + FIntVector(16);
	const auto Cell = [](const int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
	const int64 Horizontal = int64(MaximumHaloStep) + NaturalHorizontalInfluence;
	FVoxelCoverageLocalSnapshot Local;
	GeometryIndex.Capture(FIntVector(Cell(int64(EditMin.X) - Horizontal), Cell(int64(EditMin.Y) - Horizontal), Cell(int64(EditMin.Z) - MaximumHaloStep)),
		FIntVector(Cell(int64(EditMax.X) + Horizontal), Cell(int64(EditMax.Y) + Horizontal), Cell(int64(EditMax.Z) + MaximumHaloStep + NaturalUpwardInfluence)), Local);
	for (const auto& Record : Local.Records)
	{
		const auto& Key = Record->Geometry.Key;
		if (!AffectsGeometry(Key, InSection)) continue;
		switch (Key.Representation)
		{
		case EVoxelTerrainRepresentation::VoxelProxy: Result.Proxy.Add({Key.Coordinate, Key.Level}); break;
		case EVoxelTerrainRepresentation::Surface: Result.Surface.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}); break;
		case EVoxelTerrainRepresentation::Macro: Result.Macro.Add({FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level}); break;
		default: break;
		}
	}
	return Result;
}

bool FVoxelChangeHierarchy::AffectsGeometry(const FVoxelTerrainSpatialKey& InKey, const FIntVector& InSection) const
{
	const auto Record = GeometryIndex.Find(InKey);
	if (!Record) return false;
	const int64 Step = RetainedHaloSteps.FindChecked(InKey);
	const int64 Margin = Step + NaturalHorizontalInfluence;
	const FIntVector EditMin = InSection * 16;
	const FIntVector EditMax = EditMin + FIntVector(16);
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
	FVoxelCoverageLocalSnapshot Local;
	SectionChangeIndex.Capture(FIntVector(Cell(int64(Min.X) - Horizontal), Cell(int64(Min.Y) - Horizontal),
		Cell(int64(Min.Z) - InRetainedHaloStep - NaturalUpwardInfluence)),
		FIntVector(Cell(int64(Max.X) + Horizontal), Cell(int64(Max.Y) + Horizontal), Cell(int64(Max.Z) + InRetainedHaloStep)), Local);
	uint64 Revision = 0;
	for (const auto& Record : Local.Records) Revision = FMath::Max(Revision, Record->Stamp.ContentRevision);
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

void FVoxelChangeHierarchy::SetVoxelProxyRevision(
	const FVoxelViewKey& InProxyKey,
	const uint64 InRevision)
{
	FWriteScopeLock Scope(Lock);
	VoxelProxyRevision.Add(InProxyKey, InRevision);
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
}

void FVoxelChangeHierarchy::SetSurfaceRevision(
	const FVoxelSurfaceTileKey& InSurfaceTile,
	const uint64 InRevision)
{
	FWriteScopeLock Scope(Lock);
	SurfaceRevision.Add(InSurfaceTile, InRevision);
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
}

void FVoxelChangeHierarchy::SetMacroRevision(
	const FVoxelMacroTileKey& InMacroTile,
	const uint64 InRevision)
{
	FWriteScopeLock Scope(Lock);
	MacroRevision.Add(InMacroTile, InRevision);
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
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

void FVoxelChangeHierarchy::Reset()
{
	FWriteScopeLock Scope(Lock);
	GeometryIndex.Reset();
	SectionChangeIndex.Reset();
	FineShardIndex.Reset();
	FineShardRevisions.Reset();
	FineShardReferences.Reset();
	RetainedHaloSteps.Reset(); HaloStepCounts.Reset(); MaximumHaloStep = 1;
	RevisionSerial = 0;
	SectionRevision.Reset();
	VoxelProxyRevision.Reset();
	SurfaceRevision.Reset();
	MacroRevision.Reset();
}
