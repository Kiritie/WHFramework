#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Streaming/VoxelInterest.h"

class AActor;
class FVoxelTaskScheduler;
class UVoxelMeshComponent;
class UVoxelModule;
struct FVoxelMacroTileData;
struct FVoxelSectionMeshResult;
struct FVoxelTaskResult;
struct FVoxelVoxelProxyData;
struct FVoxelWaterSurfaceTileData;
struct FVoxelRepresentationReply;
struct FVoxelRepresentationInvalidate;

class WHFRAMEWORK_API FVoxelViewManager : public IVoxelOverlaySource
{
public:
	FVoxelViewManager(
		UVoxelModule& InModule,
		FVoxelTaskScheduler& InScheduler,
		uint64 InWorldEpoch);
	~FVoxelViewManager();

	void Tick(TConstArrayView<FVector> InObservers);
	bool OnTask(FVoxelTaskResult&& InResult);
	void InvalidateSection(const FIntVector& InKey);
	bool ApplyRemoteRepresentation(const FVoxelRepresentationReply& InReply, FString& OutError);
	void InvalidateRemoteRepresentations(const FVoxelRepresentationInvalidate& InInvalidate);
	void Reset();
	bool HasPrimaryRepresentation() const;

	virtual void EnumerateModifiedSections(
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections) const override;
	virtual bool ReadOverlay(
		const FIntVector& InSection,
		FVoxelOverlaySnapshot& OutOverlay) const override;

private:
	void UpdateFineAndVoxelProxy(TConstArrayView<FVector> InObservers);
	void UpdateSurface(TConstArrayView<FVector> InObservers);
	void UpdateMacro(TConstArrayView<FVector> InObservers);
	void RequestFine(const FIntVector& InSection, uint64 InRevision);
	void RequestVoxelProxy(const FVoxelViewKey& InKey);
	void RequestSurface(const FVoxelSurfaceTileKey& InKey);
	void RequestMacro(const FVoxelMacroTileKey& InKey);
	void PublishFine(const FVoxelTaskResult& InResult);
	void PublishVoxelProxy(const FVoxelTaskResult& InResult);
	void PublishSurface(const FVoxelTaskResult& InResult);
	void PublishWater(const FVoxelTaskResult& InResult);
	void PublishMacro(const FVoxelTaskResult& InResult);
	bool PublishMeshActor(
		AActor*& InOutActor,
		const FVector& InLocation,
		double InBlockSize,
		const FVoxelSectionMeshResult& InMesh);

private:
	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	TSet<FIntVector> FineWanted;
	TSet<FVoxelViewKey> VoxelProxyWanted;
	TSet<FVoxelSurfaceTileKey> SurfaceWanted;
	TSet<FVoxelMacroTileKey> MacroWanted;
	TMap<FIntVector, TObjectPtr<AActor>> FineActors;
	TMap<FIntVector, uint64> FineRevisions;
	TMap<FVoxelViewKey, TObjectPtr<AActor>> VoxelProxyActors;
	TMap<FVoxelViewKey, uint64> VoxelProxyRevisions;
	TMap<FVoxelSurfaceTileKey, TObjectPtr<AActor>> SurfaceActors;
	TMap<FVoxelSurfaceTileKey, TObjectPtr<AActor>> WaterActors;
	TMap<FVoxelMacroTileKey, TObjectPtr<AActor>> MacroActors;
	TMap<FVoxelSurfaceTileKey, uint64> SurfaceRevisions;
	TMap<FVoxelMacroTileKey, uint64> MacroRevisions;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelVoxelProxyData>> VoxelProxyData;
	TMap<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelSurfaceTileData>> SurfaceData;
	TMap<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelWaterSurfaceTileData>> WaterData;
	TMap<FVoxelMacroTileKey, TSharedPtr<const FVoxelMacroTileData>> MacroData;
};
