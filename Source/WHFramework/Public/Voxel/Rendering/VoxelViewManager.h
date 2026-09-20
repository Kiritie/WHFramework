#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Streaming/VoxelInterest.h"

class AActor;
class FVoxelTaskScheduler;
class UVoxelMeshComponent;
class UVoxelModule;

struct FVoxelMacroTileData;
struct FVoxelCoverageBox;
struct FVoxelCoverageRect;
struct FVoxelRepresentationInvalidate;
struct FVoxelRepresentationReply;
struct FVoxelSectionMeshResult;
struct FVoxelSectionSnapshot;
struct FVoxelTaskResult;
struct FVoxelVoxelProxyData;
struct FVoxelWaterSurfaceTileData;

struct WHFRAMEWORK_API FVoxelPrimaryFineReadiness
{
	int32 Required = 0;
	int32 Ready = 0;
	int32 Renderable = 0;

	bool IsComplete() const
	{
		return Required > 0 && Ready >= Required && Renderable > 0;
	}
};

class WHFRAMEWORK_API FVoxelViewManager :
	public IVoxelOverlaySource
{
public:
	FVoxelViewManager(
		UVoxelModule& InModule,
		FVoxelTaskScheduler& InScheduler,
		uint64 InWorldEpoch);

	~FVoxelViewManager();

	void Tick(
		uint64 InInterestRevision,
		TConstArrayView<FVector> InObservers);
	bool OnTask(FVoxelTaskResult&& InResult);
	void InvalidateSection(const FIntVector& InKey);

	bool ApplyRemoteRepresentation(
		const FVoxelRepresentationReply& InReply,
		FString& OutError);

	void InvalidateRemoteRepresentations(
		const FVoxelRepresentationInvalidate& InInvalidate);

	void Reset();
	bool HasPrimaryRepresentation() const;
	FVoxelPrimaryFineReadiness GetPrimaryFineReadiness(
		const TMap<FIntVector, FVoxelExactDemand>& InExact) const;

	virtual bool EnumerateModifiedSections(
		const FVoxelGenerationBounds& InBounds,
		TArray<FIntVector>& OutSections,
		const TAtomic<bool>* InCancel = nullptr) const override;

	virtual bool ReadOverlay(
		const FIntVector& InSection,
		FVoxelOverlaySnapshot& OutOverlay) const override;

private:
	void UpdateFineAndVoxelProxy(TConstArrayView<FVector> InObservers);
	void UpdateSurface(TConstArrayView<FVector> InObservers);
	void UpdateMacro(TConstArrayView<FVector> InObservers);
	void UpdateWantedTimestamps(double InNow);
	void ProcessAdmissions();
	void SetActorHiddenCached(AActor* InActor, bool bInHidden);
	void LogRepresentationState(TConstArrayView<FVector> InObservers);
	void ResolveTransitionVisibility();
	void CleanupRetiredRepresentations(double InNow);

	FVoxelCoverageBox FineCoverageBox(const FIntVector& InKey) const;
	FVoxelCoverageBox VoxelProxyCoverageBox(const FVoxelViewKey& InKey) const;
	FVoxelCoverageRect FineCoverageRect(const FIntVector& InKey) const;
	FVoxelCoverageRect VoxelProxyCoverageRect(const FVoxelViewKey& InKey) const;
	FVoxelCoverageRect SurfaceCoverageRect(const FVoxelSurfaceTileKey& InKey) const;
	FVoxelCoverageRect MacroCoverageRect(const FVoxelMacroTileKey& InKey) const;
	void GatherReadyWantedFineBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const;
	void GatherReadyWantedProxyBoxes(TArray<FVoxelCoverageBox>& OutCoverage) const;
	void GatherReadyWantedSurfaceRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	void GatherReadyWantedMacroRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	void GatherReadyWantedProxySurfaceRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	void GatherCurrentRenderDomainRects(TArray<FVoxelCoverageRect>& OutCoverage) const;
	bool IsFineReplacementReady(const FIntVector& InKey) const;
	bool IsProxyReplacementReady(const FVoxelViewKey& InKey) const;
	bool IsSurfaceReplacementReady(const FVoxelSurfaceTileKey& InKey) const;
	bool IsMacroReplacementReady(const FVoxelMacroTileKey& InKey) const;
	bool IsFineInsideRenderDomain(const FIntVector& InKey) const;
	bool IsProxyInsideRenderDomain(const FVoxelViewKey& InKey) const;
	bool IsSurfaceInsideRenderDomain(const FVoxelSurfaceTileKey& InKey) const;
	bool IsMacroInsideRenderDomain(const FVoxelMacroTileKey& InKey) const;
	static bool HasRenderableMesh(const FVoxelSectionMeshResult& InMesh);
	static bool BuildVoxelProxySnapshot(
		const FVoxelVoxelProxyData& InData,
		uint64 InRecipeHash,
		FVoxelSectionSnapshot& OutSnapshot,
		FString& OutError);

	void RequestFine(const FIntVector& InSection, uint64 InRevision);
	void RequestVoxelProxy(const FVoxelViewKey& InKey);
	void RequestSurface(const FVoxelSurfaceTileKey& InKey);
	void RequestMacro(const FVoxelMacroTileKey& InKey);

	bool PublishFine(const FVoxelTaskResult& InResult);
	bool PublishVoxelProxy(const FVoxelTaskResult& InResult);
	bool PublishSurface(const FVoxelTaskResult& InResult);
	void PublishWater(const FVoxelTaskResult& InResult);
	bool PublishMacro(const FVoxelTaskResult& InResult);

	bool PublishMeshActor(
		AActor*& InOutActor,
		const FVector& InLocation,
		double InBlockSize,
		const FVoxelSectionMeshResult& InMesh);

private:
	static constexpr double RetireDelaySeconds = 0.20;
	static constexpr double OutsideDomainRetireDelaySeconds = 0.50;

	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 AppliedInterestRevision = 0;
	int32 FineAdmissionIndex = 0;
	int32 VoxelProxyAdmissionIndex = 0;
	int32 SurfaceAdmissionIndex = 0;
	int32 MacroAdmissionIndex = 0;
	TArray<FIntVector> FineAdmissions;
	TArray<FVoxelViewKey> VoxelProxyAdmissions;
	TArray<FVoxelSurfaceTileKey> SurfaceAdmissions;
	TArray<FVoxelMacroTileKey> MacroAdmissions;
	bool bCoverageDirty = true;
	double NextCoverageCheck = 0.0;
	double NextRepresentationDebugLog = 0.0;
	TMap<TWeakObjectPtr<AActor>, bool> ActorHiddenStates;

	TSet<FIntVector> FineWanted;
	TSet<FVoxelViewKey> VoxelProxyWanted;
	TSet<FVoxelSurfaceTileKey> SurfaceWanted;
	TSet<FVoxelMacroTileKey> MacroWanted;
	TSet<FIntVector> FineReady;
	TSet<FVoxelViewKey> VoxelProxyReady;
	TSet<FVoxelSurfaceTileKey> SurfaceReady;
	TSet<FVoxelMacroTileKey> MacroReady;

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

	TMap<FIntVector, double> FineLastWanted;
	TMap<FVoxelViewKey, double> VoxelProxyLastWanted;
	TMap<FVoxelSurfaceTileKey, double> SurfaceLastWanted;
	TMap<FVoxelMacroTileKey, double> MacroLastWanted;
};
