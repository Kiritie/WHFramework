#include "Voxel/Runtime/VoxelChangeHierarchy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"

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
	bUsesActualGeometry = true;
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

void FVoxelChangeHierarchy::SetTopology(const FVoxelLodTopologySettings& InLod)
{
	FWriteScopeLock Scope(Lock);
	Lod = InLod;
}

uint64 FVoxelChangeHierarchy::InvalidateSection(const FIntVector& InSection)
{
	const FVoxelAffectedRepresentationKeys Keys = GatherAffectedKeys(InSection);
	FWriteScopeLock Scope(Lock);
	const uint64 Revision = ++RevisionSerial;
	SectionRevision.Add(InSection, Revision);
	for (const auto& Key : Keys.Proxy) VoxelProxyRevision.Add(Key, Revision);
	for (const auto& Key : Keys.Surface) SurfaceRevision.Add(Key, Revision);
	for (const auto& Key : Keys.Macro) MacroRevision.Add(Key, Revision);
	return Revision;
}

FVoxelAffectedRepresentationKeys FVoxelChangeHierarchy::GatherAffectedKeys(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	FVoxelAffectedRepresentationKeys Result;
	if (bUsesActualGeometry)
	{
		const FIntVector EditMin = InSection * 16;
		const FIntVector EditMax = EditMin + FIntVector(16);
		const auto Cell = [](int64 Value) { return int32(FMath::Clamp<int64>(Value, MIN_int32, MAX_int32)); };
		const int64 Horizontal = int64(MaximumHaloStep) + NaturalHorizontalInfluence;
		FVoxelCoverageLocalSnapshot Local;
		GeometryIndex.Capture(FIntVector(Cell(int64(EditMin.X) - Horizontal), Cell(int64(EditMin.Y) - Horizontal), Cell(int64(EditMin.Z) - MaximumHaloStep)),
			FIntVector(Cell(int64(EditMax.X) + Horizontal), Cell(int64(EditMax.Y) + Horizontal), Cell(int64(EditMax.Z) + MaximumHaloStep + NaturalUpwardInfluence)), Local);
		for (const auto& Record : Local.Records)
		{
			const auto& Key = Record->Geometry.Key;
			const int64 Step = RetainedHaloSteps.FindChecked(Key);
			const int64 Margin = Step + NaturalHorizontalInfluence;
			if (int64(Record->Min.X) - Margin >= EditMax.X || int64(Record->Max.X) + Margin <= EditMin.X ||
				int64(Record->Min.Y) - Margin >= EditMax.Y || int64(Record->Max.Y) + Margin <= EditMin.Y ||
				int64(Record->Min.Z) - Step - NaturalUpwardInfluence >= EditMax.Z || int64(Record->Max.Z) + Step <= EditMin.Z) continue;
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
	const FIntVector CellMin = InSection * 16;
	const FIntVector CellMax = CellMin + FIntVector(15);
	for (uint8 Level = 0; Level <= Lod.MaximumVoxelProxyLevel; ++Level)
	{
		const int32 Step = FVoxelViewKey{ FIntVector::ZeroValue, Level }.GetSampleStep();
		const int32 ProxySide = FVoxelViewKey{ FIntVector::ZeroValue, Level }.GetSide();
		FIntVector ProxyMin;
		FIntVector ProxyMax;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int32 SideInfluence = Axis == 2 ? 0 : NaturalHorizontalInfluence;
			const int32 UpperInfluence = Axis == 2 ?
				NaturalUpwardInfluence : NaturalHorizontalInfluence;
			ProxyMin[Axis] = VoxelGeneration::FloorDivide(
				CellMin[Axis] - Step - SideInfluence, ProxySide);
			ProxyMax[Axis] = VoxelGeneration::FloorDivide(
				CellMax[Axis] + Step + UpperInfluence, ProxySide);
		}
		for (int32 Z = ProxyMin.Z; Z <= ProxyMax.Z; ++Z)
		{
			for (int32 Y = ProxyMin.Y; Y <= ProxyMax.Y; ++Y)
			{
				for (int32 X = ProxyMin.X; X <= ProxyMax.X; ++X)
				{
					Result.Proxy.Add({FIntVector(X, Y, Z), Level});
				}
			}
		}
	}
	for (uint8 Level = 0; Level <= Lod.MaximumSurfaceSpatialLevel; ++Level)
	{
		const int32 SurfaceSide = VoxelViewLod::SurfaceSide(Lod, Level);
		const int32 SurfaceMargin = FMath::Max(VoxelViewLod::SurfaceStep(Lod, Level), NaturalHorizontalInfluence);
		for (int32 Y = VoxelGeneration::FloorDivide(CellMin.Y - SurfaceMargin, SurfaceSide);
			Y <= VoxelGeneration::FloorDivide(CellMax.Y + SurfaceMargin, SurfaceSide); ++Y)
		{
			for (int32 X = VoxelGeneration::FloorDivide(CellMin.X - SurfaceMargin, SurfaceSide);
				X <= VoxelGeneration::FloorDivide(CellMax.X + SurfaceMargin, SurfaceSide); ++X)
			{
				Result.Surface.Add({FIntPoint(X, Y), Level});
			}
		}
	}
	for (uint8 Level = 0; Level <= Lod.MaximumMacroSpatialLevel; ++Level)
	{
		const int32 MacroSide = VoxelViewLod::MacroSide(Lod, Level);
		const int32 MacroMargin = FMath::Max(VoxelViewLod::MacroStep(Lod, Level), NaturalHorizontalInfluence);
		for (int32 Y = VoxelGeneration::FloorDivide(CellMin.Y - MacroMargin, MacroSide);
			Y <= VoxelGeneration::FloorDivide(CellMax.Y + MacroMargin, MacroSide); ++Y)
		{
			for (int32 X = VoxelGeneration::FloorDivide(CellMin.X - MacroMargin, MacroSide);
				X <= VoxelGeneration::FloorDivide(CellMax.X + MacroMargin, MacroSide); ++X)
			{
				Result.Macro.Add({FIntPoint(X, Y), Level});
			}
		}
	}
	return Result;
}

uint64 FVoxelChangeHierarchy::GetSectionRevision(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = SectionRevision.Find(InSection);
	return Found ? *Found : 0;
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

bool FVoxelChangeHierarchy::AffectsVoxelProxy(
	const FVoxelViewKey& InKey,
	const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	if (bUsesActualGeometry)
	{
		FVoxelCoverageLocalSnapshot Local;
		GeometryIndex.Capture(InSection * 16, (InSection + FIntVector(1)) * 16, Local);
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::VoxelProxy, InKey.Coordinate, InKey.Level};
		return Local.Records.ContainsByPredicate([&Key](const auto& Record) { return Record->Geometry.Key == Key; });
	}
	const FVoxelGenerationBounds SectionBounds { InSection * 16, (InSection + FIntVector(1)) * 16 };
	FVoxelGenerationBounds Bounds = InKey.GetBounds();
	Bounds.Min -= FIntVector(InKey.GetSampleStep() + NaturalHorizontalInfluence,
		InKey.GetSampleStep() + NaturalHorizontalInfluence,
		InKey.GetSampleStep() + NaturalUpwardInfluence);
	Bounds.Max += FIntVector(InKey.GetSampleStep() + NaturalHorizontalInfluence,
		InKey.GetSampleStep() + NaturalHorizontalInfluence, InKey.GetSampleStep());
	return Bounds.Intersects(SectionBounds);
}

bool FVoxelChangeHierarchy::AffectsSurface(
	const FVoxelSurfaceTileKey& InKey,
	const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	if (bUsesActualGeometry)
	{
		FVoxelCoverageLocalSnapshot Local;
		GeometryIndex.Capture(InSection * 16, (InSection + FIntVector(1)) * 16, Local);
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Surface, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level};
		return Local.Records.ContainsByPredicate([&Key](const auto& Record) { return Record->Geometry.Key == Key; });
	}
	const int32 Side = VoxelViewLod::SurfaceSide(Lod, InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	const FIntVector SectionMin = InSection * 16;
	const int32 Margin = FMath::Max(VoxelViewLod::SurfaceStep(Lod, InKey.Level), NaturalHorizontalInfluence);
	return SectionMin.X < Min.X + Side + Margin && SectionMin.X + 16 > Min.X - Margin &&
		SectionMin.Y < Min.Y + Side + Margin && SectionMin.Y + 16 > Min.Y - Margin;
}

bool FVoxelChangeHierarchy::AffectsMacro(
	const FVoxelMacroTileKey& InKey,
	const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	if (bUsesActualGeometry)
	{
		FVoxelCoverageLocalSnapshot Local;
		GeometryIndex.Capture(InSection * 16, (InSection + FIntVector(1)) * 16, Local);
		const FVoxelTerrainSpatialKey Key{EVoxelTerrainRepresentation::Macro, FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0), InKey.Level};
		return Local.Records.ContainsByPredicate([&Key](const auto& Record) { return Record->Geometry.Key == Key; });
	}
	const int32 Side = VoxelViewLod::MacroSide(Lod, InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	const FIntVector SectionMin = InSection * 16;
	const int32 Step = VoxelViewLod::MacroStep(Lod, InKey.Level);
	const int32 Margin = FMath::Max(Step, NaturalHorizontalInfluence);
	return SectionMin.X < Min.X + Side + Margin && SectionMin.X + 16 > Min.X - Margin &&
		SectionMin.Y < Min.Y + Side + Margin && SectionMin.Y + 16 > Min.Y - Margin;
}

void FVoxelChangeHierarchy::ReleaseSection(const FIntVector& InSection)
{
	FWriteScopeLock Scope(Lock);
	SectionRevision.Remove(InSection);
}

void FVoxelChangeHierarchy::Reset()
{
	FWriteScopeLock Scope(Lock);
	GeometryIndex.Reset();
	RetainedHaloSteps.Reset(); HaloStepCounts.Reset(); MaximumHaloStep = 1;
	bUsesActualGeometry = false;
	RevisionSerial = 0;
	SectionRevision.Reset();
	VoxelProxyRevision.Reset();
	SurfaceRevision.Reset();
	MacroRevision.Reset();
}
