#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/DWLodTransition.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Streaming/VoxelInterest.h"

class AActor;
class FVoxelTaskScheduler;
class FVoxelViewPublisher;
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
struct FVoxelBoundaryTransitionContext;

struct WHFRAMEWORK_API FVoxelPrimaryFineReadiness
{
	int32 Required = 0;
	int32 Ready = 0;
	int32 Renderable = 0;
	int32 Presented = 0;

	bool IsComplete() const
	{
		return Required > 0 && Ready >= Required && Presented >= Required;
	}
};

class WHFRAMEWORK_API FVoxelViewManager
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
	void InvalidateNeighbors(const FIntVector& InKey);

	bool ApplyRemoteRepresentation(
		const FVoxelRepresentationReply& InReply,
		FString& OutError);

	void InvalidateRemoteRepresentations(
		const FVoxelRepresentationInvalidate& InInvalidate);

	void Reset();
	bool HasPrimaryRepresentation() const;
	bool RequiresSectionData(const FIntVector& InKey) const;
	double GetDataAdmissionLimit() const;
	FVoxelPrimaryFineReadiness GetPrimaryFineReadiness(
		const TMap<FIntVector, FVoxelExactDemand>& InExact) const;
	FVoxelPrimaryFineReadiness GetFineRadiusReadiness(
		TConstArrayView<FIntVector> InFineKeys) const;
	static void SortAdmissionsByPriority(TArray<FVoxelViewAdmission>& InOutAdmissions);
	static int32 ResolveActiveAdmissionKind(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady);
	static double ResolveAdmissionFrontier(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
		double InBandWidthCells);

private:
	void UpdateFineAndVoxelProxy(TConstArrayView<FVector> InObservers);
	void UpdateSurface(TConstArrayView<FVector> InObservers);
	void UpdateMacro(TConstArrayView<FVector> InObservers);
	void UpdateWantedTimestamps(double InNow);
	void ProcessAdmissions();
	void ProcessDataAdmissions();
	bool IsAdmissionDataReady(const FVoxelViewAdmission& InAdmission) const;
	bool IsAdmissionMeshReady(const FVoxelViewAdmission& InAdmission) const;
	bool IsPreparedDataCurrent(const FVoxelTaskKey& InKey) const;
	void PrunePreparedData();
	bool EnqueuePreparedData(FVoxelTaskRequest&& InRequest);
	void RemovePreparedData(const FVoxelTaskKey& InKey);
	void RebuildAdmissions(TConstArrayView<FVector> InObservers);
	void UpdateTaskPriorities();
	EVoxelWorkClass VolumeTransitionWorkClass(const FVoxelViewKey& InOwner) const;
	void TrackReadyTerrainNode(FVoxelViewKey InKey);
	void RebuildReadyTerrainBranches();
	double MinimumObserverDistanceCells(const FVector& InWorldCenter) const;
	double MinimumObserverDistanceCells(const FVoxelGenerationBounds& InCellBounds) const;
	bool IsAdmissionSatisfied(const FVoxelViewAdmission& InAdmission) const;
	bool IsAdmissionTerminalFailure(const FVoxelViewAdmission& InAdmission) const;
	bool TrySubmitAdmission(const FVoxelViewAdmission& InAdmission);
	void CancelStaleViewTasks();
	FVector SurfaceWorldCenter(const FVoxelSurfaceTileKey& InKey) const;
	FVector MacroWorldCenter(const FVoxelMacroTileKey& InKey) const;
	void LogRepresentationState(TConstArrayView<FVector> InObservers);
	void ResolveTransitionVisibility();
	void RebuildHeightfieldTransitions(
		const TArray<FBox>& InFineBoxes,
		const TArray<FBox>& InProxySurfaceBoxes);
	void ApplyHeightfieldTransition(
		const FVoxelHeightfieldNodeKey& InOwner,
		uint64 InSignature,
		FVoxelTaskResult&& InResult);
	void PumpHeightfieldTransitions();
	bool RebuildVolumeTransitions(const TSet<FVoxelViewKey>& InTargetNodes);
	void PumpVolumeTransitions();
	void ApplyVolumeTransition(const FVoxelViewKey& InOwner, uint64 InSignature,
		FVoxelTaskResult&& InResult);
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
	void GatherReadyWantedProxySurfaceRects(const FVoxelCoverageRect& InTarget, TArray<FVoxelCoverageRect>& OutCoverage) const;
	void UpdateProxySurfaceCoverage(const FVoxelVoxelProxyData& InData);
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
	bool TryResolveFineWithoutMesh(const FIntVector& InSection, uint64 InRevision);
	static bool BuildVoxelProxySnapshot(
		const FVoxelVoxelProxyData& InData,
		uint64 InRecipeHash,
		FVoxelSectionSnapshot& OutSnapshot,
		FString& OutError);

	bool RequestFine(const FIntVector& InSection, uint64 InRevision);
	bool RequestVoxelProxy(const FVoxelViewKey& InKey, bool bDataOnly = false);
	bool RequestSurface(const FVoxelSurfaceTileKey& InKey, bool bDataOnly = false);
	bool RequestMacro(const FVoxelMacroTileKey& InKey, bool bDataOnly = false);

	bool PublishFine(const FVoxelTaskResult& InResult);
	bool PublishVoxelProxy(const FVoxelTaskResult& InResult);
	bool PublishSurface(const FVoxelTaskResult& InResult);
	void PublishWater(const FVoxelTaskResult& InResult);
	bool PublishMacro(const FVoxelTaskResult& InResult);

private:
	static constexpr double RetireDelaySeconds = 0.20;
	static constexpr double OutsideDomainRetireDelaySeconds = 0.50;

	UVoxelModule& Module;
	FVoxelTaskScheduler& Scheduler;
	uint64 WorldEpoch = 0;
	uint64 AppliedInterestRevision = 0;
	TConstArrayView<FVoxelViewAdmission> Admissions;
	TArray<FVector> PriorityObservers;
	int32 AdmissionScanIndices[4] = {};
	int32 DataScanIndices[4] = {};
	TMap<FVoxelTaskKey, TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe>> PreparedData;
	uint64 PreparedDataBytes = 0;
	uint64 PendingDataBytes = 0;
	uint64 SkippedProxyMeshes = 0;
	uint64 SkippedVolumeMeshes = 0;
	double AdmissionBandWidthCells = 64.0;
	double LastResolvedFrontier = 0.0;
	int32 LastActiveAdmissionKind = 4;
	int32 LastActiveDataKind = 4;
	bool bCoverageDirty = true;
	double NextRetireCheck = 0.0;
	double LastCoverageMilliseconds = 0.0;
	double LastRetireMilliseconds = 0.0;
	double NextRepresentationDebugLog = 0.0;
	TUniquePtr<FVoxelViewPublisher> Publisher;

	TSet<FIntVector> FineWanted;
	TSet<FVoxelViewKey> VisibleTerrainNodes;
	TSet<FVoxelViewKey> ReadyTerrainBranches;
	bool bReadyTerrainBranchesDirty = true;
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
	TMap<FVoxelHeightfieldNodeKey, TObjectPtr<AActor>> HeightfieldTransitionActors;
	TMap<FVoxelHeightfieldNodeKey, uint64> HeightfieldTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, uint64> DesiredTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, uint64> PendingTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, uint64> PreparedTransitionSignatures;
	TMap<FVoxelHeightfieldNodeKey, TSharedPtr<FVoxelSectionMeshResult>> PreparedTransitionMeshes;
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldTransitionEdge>> DesiredTransitionEdges;
	TArray<FVoxelHeightfieldNodeKey> UnsubmittedTransitionOwners;
	TArray<FVoxelCoverageRect> PendingHeightfieldHandoffRects;
	int32 HeightfieldUnbalancedEdgeCount = 0;
	TMap<FVoxelViewKey, uint64> VolumeTransitionSignatures;
	TMap<FVoxelViewKey, uint64> DesiredVolumeSignatures;
	TMap<FVoxelViewKey, uint64> PendingVolumeSignatures;
	TMap<FVoxelViewKey, uint64> PreparedVolumeSignatures;
	TMap<FVoxelViewKey, TSharedPtr<FVoxelSectionMeshResult>> PreparedVolumeMeshes;
	TMap<FVoxelViewKey, TSharedPtr<const FVoxelBoundaryTransitionContext>> DesiredVolumeContexts;
	TArray<FVoxelViewKey> UnsubmittedVolumeOwners;
	int32 VolumeUnbalancedFaceCount = 0;
	int32 MissingFineBoundaryCount = 0;
	int32 MissingProxyBoundaryCount = 0;
	int32 InvalidVolumeContextCount = 0;

	TMap<FVoxelSurfaceTileKey, uint64> SurfaceRevisions;
	TMap<FVoxelMacroTileKey, uint64> MacroRevisions;

	TMap<FVoxelViewKey, TSharedPtr<const FVoxelVoxelProxyData>> VoxelProxyData;
	TMap<FVoxelViewKey, TArray<FVoxelCoverageRect>> ProxySurfaceCoverage;
	TMap<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelSurfaceTileData>> SurfaceData;
	TMap<FVoxelSurfaceTileKey, TSharedPtr<const FVoxelWaterSurfaceTileData>> WaterData;
	TMap<FVoxelMacroTileKey, TSharedPtr<const FVoxelMacroTileData>> MacroData;

	TMap<FIntVector, double> FineLastWanted;
	TMap<FVoxelViewKey, double> VoxelProxyLastWanted;
	TMap<FVoxelSurfaceTileKey, double> SurfaceLastWanted;
	TMap<FVoxelMacroTileKey, double> MacroLastWanted;
};
