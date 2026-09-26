#include "Voxel/Runtime/VoxelChangeHierarchy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
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

uint64 FVoxelChangeHierarchy::InvalidateSection(const FIntVector& InSection)
{
	FWriteScopeLock Scope(Lock);
	const uint64 Revision = ++RevisionSerial;
	SectionRevision.Add(InSection, Revision);
	const FIntVector CellMin = InSection * 16;
	const FIntVector CellMax = CellMin + FIntVector(15);
	for (uint8 Level = 0; Level <= 8; ++Level)
	{
		const int32 Step = 1 << Level;
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
					VoxelProxyRevision.Add({ FIntVector(X, Y, Z), Level }, Revision);
				}
			}
		}
		const int32 SurfaceSide = FVoxelSurfaceTileData::CellSide * Step;
		const int32 MacroStep = FVoxelMacroTileData::BaseStep * Step;
		const int32 MacroSide = FVoxelMacroTileData::CellSide * MacroStep;
		const int32 SurfaceMargin = FMath::Max(Step, NaturalHorizontalInfluence);
		const int32 MacroMargin = FMath::Max(MacroStep, NaturalHorizontalInfluence);
		for (int32 Y = VoxelGeneration::FloorDivide(CellMin.Y - SurfaceMargin, SurfaceSide);
			Y <= VoxelGeneration::FloorDivide(CellMax.Y + SurfaceMargin, SurfaceSide); ++Y)
		{
			for (int32 X = VoxelGeneration::FloorDivide(CellMin.X - SurfaceMargin, SurfaceSide);
				X <= VoxelGeneration::FloorDivide(CellMax.X + SurfaceMargin, SurfaceSide); ++X)
			{
				SurfaceRevision.Add({ FIntPoint(X, Y), Level }, Revision);
			}
		}
		for (int32 Y = VoxelGeneration::FloorDivide(CellMin.Y - MacroMargin, MacroSide);
			Y <= VoxelGeneration::FloorDivide(CellMax.Y + MacroMargin, MacroSide); ++Y)
		{
			for (int32 X = VoxelGeneration::FloorDivide(CellMin.X - MacroMargin, MacroSide);
				X <= VoxelGeneration::FloorDivide(CellMax.X + MacroMargin, MacroSide); ++X)
			{
				MacroRevision.Add({ FIntPoint(X, Y), Level }, Revision);
			}
		}
	}
	return Revision;
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
	const FVoxelGenerationBounds SectionBounds { InSection * 16, (InSection + FIntVector(1)) * 16 };
	FVoxelGenerationBounds Bounds = InKey.GetBounds();
	Bounds.Min -= FIntVector(InKey.GetStep() + NaturalHorizontalInfluence,
		InKey.GetStep() + NaturalHorizontalInfluence,
		InKey.GetStep() + NaturalUpwardInfluence);
	Bounds.Max += FIntVector(InKey.GetStep() + NaturalHorizontalInfluence,
		InKey.GetStep() + NaturalHorizontalInfluence, InKey.GetStep());
	return Bounds.Intersects(SectionBounds);
}

bool FVoxelChangeHierarchy::AffectsSurface(
	const FVoxelSurfaceTileKey& InKey,
	const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const int32 Side = 32 * (1 << InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	const FIntVector SectionMin = InSection * 16;
	const int32 Margin = FMath::Max(1 << InKey.Level, NaturalHorizontalInfluence);
	return SectionMin.X < Min.X + Side + Margin && SectionMin.X + 16 > Min.X - Margin &&
		SectionMin.Y < Min.Y + Side + Margin && SectionMin.Y + 16 > Min.Y - Margin;
}

bool FVoxelChangeHierarchy::AffectsMacro(
	const FVoxelMacroTileKey& InKey,
	const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const int32 Side = 32 * (64 << InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	const FIntVector SectionMin = InSection * 16;
	const int32 Step = FVoxelMacroTileData::BaseStep << InKey.Level;
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
	RevisionSerial = 0;
	SectionRevision.Reset();
	VoxelProxyRevision.Reset();
	SurfaceRevision.Reset();
	MacroRevision.Reset();
}
