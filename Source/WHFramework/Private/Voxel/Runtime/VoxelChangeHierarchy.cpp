#include "Voxel/Runtime/VoxelChangeHierarchy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

uint64 FVoxelChangeHierarchy::InvalidateSection(const FIntVector& InSection)
{
	FWriteScopeLock Scope(Lock);
	const uint64 Revision = ++RevisionSerial;
	SectionRevision.Add(InSection, Revision);
	VoxelProxyRevision.Add(ToVoxelProxyParent(InSection), Revision);
	SurfaceRevision.Add(ToSurfaceParent(InSection), Revision);
	MacroRevision.Add(ToMacroParent(InSection), Revision);
	return Revision;
}

uint64 FVoxelChangeHierarchy::GetSectionRevision(const FIntVector& InSection) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = SectionRevision.Find(InSection);
	return Found ? *Found : 0;
}

uint64 FVoxelChangeHierarchy::GetVoxelProxyRevision(const FIntVector& InProxyKey) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = VoxelProxyRevision.Find(InProxyKey);
	return Found ? *Found : 0;
}

uint64 FVoxelChangeHierarchy::GetSurfaceRevision(const FIntPoint& InSurfaceTile) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = SurfaceRevision.Find(InSurfaceTile);
	return Found ? *Found : 0;
}

uint64 FVoxelChangeHierarchy::GetMacroRevision(const FIntPoint& InMacroTile) const
{
	FReadScopeLock Scope(Lock);
	const uint64* Found = MacroRevision.Find(InMacroTile);
	return Found ? *Found : 0;
}

void FVoxelChangeHierarchy::SetVoxelProxyRevision(
	const FIntVector& InProxyKey,
	const uint64 InRevision)
{
	FWriteScopeLock Scope(Lock);
	VoxelProxyRevision.Add(InProxyKey, InRevision);
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
}

void FVoxelChangeHierarchy::SetSurfaceRevision(
	const FIntPoint& InSurfaceTile,
	const uint64 InRevision)
{
	FWriteScopeLock Scope(Lock);
	SurfaceRevision.Add(InSurfaceTile, InRevision);
	RevisionSerial = FMath::Max(RevisionSerial, InRevision);
}

void FVoxelChangeHierarchy::SetMacroRevision(
	const FIntPoint& InMacroTile,
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
	const FVoxelGenerationBounds SectionBounds { InSection * 16, (InSection + FIntVector(1)) * 16 };
	return InKey.GetBounds().Intersects(SectionBounds);
}

bool FVoxelChangeHierarchy::AffectsSurface(
	const FVoxelSurfaceTileKey& InKey,
	const FIntVector& InSection) const
{
	const int32 Side = 32 * (1 << InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	const FIntVector SectionMin = InSection * 16;
	return SectionMin.X < Min.X + Side && SectionMin.X + 16 > Min.X &&
		SectionMin.Y < Min.Y + Side && SectionMin.Y + 16 > Min.Y;
}

bool FVoxelChangeHierarchy::AffectsMacro(
	const FVoxelMacroTileKey& InKey,
	const FIntVector& InSection) const
{
	const int32 Side = 32 * (64 << InKey.Level);
	const FIntPoint Min = InKey.Coordinate * Side;
	const FIntVector SectionMin = InSection * 16;
	return SectionMin.X < Min.X + Side && SectionMin.X + 16 > Min.X &&
		SectionMin.Y < Min.Y + Side && SectionMin.Y + 16 > Min.Y;
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

FIntVector FVoxelChangeHierarchy::ToVoxelProxyParent(const FIntVector& InSection) const
{
	return FIntVector(
		VoxelGeneration::FloorDivide(InSection.X, 2),
		VoxelGeneration::FloorDivide(InSection.Y, 2),
		VoxelGeneration::FloorDivide(InSection.Z, 2));
}

FIntPoint FVoxelChangeHierarchy::ToSurfaceParent(const FIntVector& InSection) const
{
	return FIntPoint(
		VoxelGeneration::FloorDivide(InSection.X, 4),
		VoxelGeneration::FloorDivide(InSection.Y, 4));
}

FIntPoint FVoxelChangeHierarchy::ToMacroParent(const FIntVector& InSection) const
{
	return FIntPoint(
		VoxelGeneration::FloorDivide(InSection.X, 32),
		VoxelGeneration::FloorDivide(InSection.Y, 32));
}
