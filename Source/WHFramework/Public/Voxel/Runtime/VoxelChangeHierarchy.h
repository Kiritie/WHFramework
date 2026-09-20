#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"

struct WHFRAMEWORK_API FVoxelChangeHierarchy
{
public:
	uint64 InvalidateSection(const FIntVector& InSection);
	uint64 GetSectionRevision(const FIntVector& InSection) const;
	uint64 GetVoxelProxyRevision(const FIntVector& InProxyKey) const;
	uint64 GetSurfaceRevision(const FIntPoint& InSurfaceTile) const;
	uint64 GetMacroRevision(const FIntPoint& InMacroTile) const;
	void SetVoxelProxyRevision(const FIntVector& InProxyKey, uint64 InRevision);
	void SetSurfaceRevision(const FIntPoint& InSurfaceTile, uint64 InRevision);
	void SetMacroRevision(const FIntPoint& InMacroTile, uint64 InRevision);
	bool AffectsVoxelProxy(const FVoxelViewKey& InKey, const FIntVector& InSection) const;
	bool AffectsSurface(const FVoxelSurfaceTileKey& InKey, const FIntVector& InSection) const;
	bool AffectsMacro(const FVoxelMacroTileKey& InKey, const FIntVector& InSection) const;
	void ReleaseSection(const FIntVector& InSection);
	void Reset();

private:
	FIntVector ToVoxelProxyParent(const FIntVector& InSection) const;
	FIntPoint ToSurfaceParent(const FIntVector& InSection) const;
	FIntPoint ToMacroParent(const FIntVector& InSection) const;

private:
	mutable FRWLock Lock;
	uint64 RevisionSerial = 0;
	TMap<FIntVector, uint64> SectionRevision;
	TMap<FIntVector, uint64> VoxelProxyRevision;
	TMap<FIntPoint, uint64> SurfaceRevision;
	TMap<FIntPoint, uint64> MacroRevision;
};
