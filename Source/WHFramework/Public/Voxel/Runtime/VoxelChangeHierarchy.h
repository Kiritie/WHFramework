#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"

struct WHFRAMEWORK_API FVoxelChangeHierarchy
{
public:
	uint64 InvalidateSection(const FIntVector& InSection);
	uint64 GetSectionRevision(const FIntVector& InSection) const;
	uint64 GetVoxelProxyRevision(const FVoxelViewKey& InProxyKey) const;
	uint64 GetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile) const;
	uint64 GetMacroRevision(const FVoxelMacroTileKey& InMacroTile) const;
	void SetVoxelProxyRevision(const FVoxelViewKey& InProxyKey, uint64 InRevision);
	void SetSurfaceRevision(const FVoxelSurfaceTileKey& InSurfaceTile, uint64 InRevision);
	void SetMacroRevision(const FVoxelMacroTileKey& InMacroTile, uint64 InRevision);
	bool AffectsVoxelProxy(const FVoxelViewKey& InKey, const FIntVector& InSection) const;
	bool AffectsSurface(const FVoxelSurfaceTileKey& InKey, const FIntVector& InSection) const;
	bool AffectsMacro(const FVoxelMacroTileKey& InKey, const FIntVector& InSection) const;
	void ReleaseSection(const FIntVector& InSection);
	void Reset();

private:
	mutable FRWLock Lock;
	uint64 RevisionSerial = 0;
	TMap<FIntVector, uint64> SectionRevision;
	TMap<FVoxelViewKey, uint64> VoxelProxyRevision;
	TMap<FVoxelSurfaceTileKey, uint64> SurfaceRevision;
	TMap<FVoxelMacroTileKey, uint64> MacroRevision;
};
