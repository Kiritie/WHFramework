#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelPublishGroups.h"
#include "Voxel/Rendering/DWLodTransition.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Streaming/VoxelInterest.h"

struct FVoxelHeightfieldCoveragePlan;
struct FVoxelVolumeCoveragePlan;
class AActor;
class FVoxelTaskScheduler;
class FVoxelGenerationPipeline;
class FVoxelGenerationCacheRetentionLease;
struct FVoxelGenerationBounds;
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
struct FVoxelInterestDelta;
struct FVoxelVoxelProxyData;
struct FVoxelWaterSurfaceTileData;
struct FVoxelBoundaryTransitionContext;

struct WHFRAMEWORK_API FVoxelPrimaryFineReadiness
{
	int32 Required = 0;
	int32 Ready = 0;
	int32 Renderable = 0;
	int32 Presented = 0;
	int32 Owned = 0;
	int32 Hidden = 0;
	int32 PublicationPending = 0;

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
	void RefreshTaskPriorities();
	void ApplyInterestDelta(const FVoxelInterestDelta& InDelta);
	FVoxelFrameTimings GetFrameTimings() const { return FrameTimings; }
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
	FVoxelRepresentationReadiness GetRepresentationReadiness() const;
	bool RequiresSectionData(const FIntVector& InKey) const;
	TMap<int32, double> GetDataAdmissionLimits() const;
	FVoxelPrimaryFineReadiness GetPrimaryFineReadiness(
		const TMap<FIntVector, FVoxelExactDemand>& InExact) const;
	FVoxelPrimaryFineReadiness GetFineRadiusReadiness(
		TConstArrayView<FIntVector> InFineKeys) const;
	static void SortAdmissionsByPriority(TArray<FVoxelViewAdmission>& InOutAdmissions);
	static TMap<int32, uint8> ResolveAdmissionLanes(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
		bool bMovementCriticalFinePending = false);
	static int32 ResolveActiveAdmissionKind(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
		int32* OutPriority = nullptr);
	static double ResolveAdmissionFrontier(
		TConstArrayView<FVoxelViewAdmission> InAdmissions,
		TFunctionRef<bool(const FVoxelViewAdmission&)> InIsReady,
		double InBandWidthCells);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelManagerLocalCoverageTest;
#endif
	void UpdateFineAndVoxelProxy(TConstArrayView<FVector> InObservers);
	void UpdateSurface(TConstArrayView<FVector> InObservers);
	void UpdateMacro(TConstArrayView<FVector> InObservers);
	void UpdateWantedTimestamps(double InNow);
	void ProcessAdmissions();
	void ProcessDataAdmissions();
	bool HasMovementCriticalFinePending() const;
	bool IsAdmissionDataReady(const FVoxelViewAdmission& InAdmission) const;
	bool IsAdmissionMeshReady(const FVoxelViewAdmission& InAdmission) const;
	bool IsPreparedDataCurrent(const FVoxelTaskKey& InKey) const;
	void PrunePreparedData();
	TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe> RetainPreparedEnvironment(
		const FVoxelTaskKey& InKey, const FVoxelGenerationPipeline& InGenerator,
		const FVoxelGenerationBounds& InBounds);
	bool EnqueuePreparedData(FVoxelTaskRequest&& InRequest);
	void RemovePreparedData(const FVoxelTaskKey& InKey);
	void RebuildAdmissions(TConstArrayView<FVector> InObservers);
	void UpdateTaskPriorities();
	EVoxelWorkClass VolumeTransitionWorkClass(const FVoxelViewKey& InOwner) const;
	void TrackReadyTerrainNode(FVoxelViewKey InKey);
	void RebuildReadyTerrainBranches();
	bool CommitTerrainOwnership(const FVoxelPublishGroupKey& InKey, TConstArrayView<FVoxelViewKey> InTarget);
	double MinimumObserverDistanceCells(const FVector& InWorldCenter) const;
	double MinimumObserverDistanceCells(const FVoxelGenerationBounds& InCellBounds) const;
	bool IsAdmissionTerminalFailure(const FVoxelViewAdmission& InAdmission) const;
	bool TrySubmitAdmission(const FVoxelViewAdmission& InAdmission);
	void CancelStaleViewTasks();
	FVector SurfaceWorldCenter(const FVoxelSurfaceTileKey& InKey) const;
	FVector MacroWorldCenter(const FVoxelMacroTileKey& InKey) const;
	void LogRepresentationState(TConstArrayView<FVector> InObservers);
	void MarkCoverageDirty();
	void MarkCoverageDirty(const FVoxelCoverageRect& InBounds);
	static void MergeCoverageRect(TArray<FVoxelCoverageRect>& InOutRects, const FVoxelCoverageRect& InBounds);
	void ExpandActiveCoverage(const FVoxelCoverageRect& InBounds);
	void RetryActiveCoverage();
	FGuid BeginPublishGroup(const FVoxelPublishGroupKey& InKey);
	bool CoverageAffects(const FVoxelCoverageRect& InBounds) const;
	void ResolveTransitionVisibility();
	bool RebuildHeightfieldTransitions();
	void ApplyHeightfieldTransition(
		const FVoxelHeightfieldNodeKey& InOwner,
		uint64 InSignature,
		FVoxelTaskResult&& InResult);
	void PumpHeightfieldTransitions();
	bool RebuildVolumeTransitions(const TSet<FVoxelViewKey>& InTargetNodes);
	bool VolumeHandoffAffectsHeightfield(const FVoxelHeightfieldNodeKey& InKey) const;
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
	bool RequestVoxelProxy(const FVoxelViewKey& InKey, int32 InPriority, bool bDataOnly = false);
	bool RequestSurface(const FVoxelSurfaceTileKey& InKey, int32 InPriority, bool bDataOnly = false);
	bool RequestMacro(const FVoxelMacroTileKey& InKey, int32 InPriority, bool bDataOnly = false);

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
	TMap<int32, int32> AdmissionScanIndices[4];
	TMap<int32, int32> DataScanIndices[4];
	TArray<int32> AdmissionPriorities;
	TMap<FVoxelTaskKey, TSharedPtr<const FVoxelTaskResult, ESPMode::ThreadSafe>> PreparedData;
	TMap<FVoxelTaskKey, TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>> PreparedEnvironmentRetentions;
	uint64 PreparedDataBytes = 0;
	uint64 PendingDataBytes = 0;
	uint64 SkippedProxyMeshes = 0;
	uint64 SkippedVolumeMeshes = 0;
	double AdmissionBandWidthCells = 64.0;
	double LastResolvedFrontier = 0.0;
	int32 LastActiveAdmissionKind = 4;
	int32 LastActiveAdmissionPriority = MAX_int32;
	int32 LastActiveDataKind = 4;
	bool bCoverageDirty = true;
	bool bCoverageDirtyFull = true;
	bool bActiveCoverageFull = true;
	TArray<FVoxelCoverageRect> DirtyCoverageRects;
	TArray<FVoxelCoverageRect> ActiveDirtyCoverageRects;
	template<typename TPlan>
	struct TLocalCoverageState
	{
		uint64 DesiredSignature = 0;
		uint64 PendingSignature = 0;
		TSharedPtr<const TPlan, ESPMode::ThreadSafe> Plan;
	};
	uint64 HeightfieldPlanSerial = 0;
	TMap<FVoxelHeightfieldNodeKey, TLocalCoverageState<FVoxelHeightfieldCoveragePlan>> HeightfieldCoveragePlans;
	TSet<FVoxelHeightfieldNodeKey> PendingHeightfieldPlanOwners;
	uint64 VolumePlanSerial = 0;
	TMap<FVoxelViewKey, TLocalCoverageState<FVoxelVolumeCoveragePlan>> VolumeCoveragePlans;
	TArray<FVoxelCoverageBox> PendingVolumeHandoffBoxes;
	double NextRetireCheck = 0.0;
	double LastCoverageMilliseconds = 0.0;
	double LastRetireMilliseconds = 0.0;
	FVoxelFrameTimings FrameTimings;
	double NextRepresentationDebugLog = 0.0;
	TUniquePtr<FVoxelViewPublisher> Publisher;
	TMap<FVoxelPublishGroupKey, FGuid> OpenPublishGroups;

	TSet<FIntVector> FineWanted;
	TSet<FVoxelViewKey> VisibleTerrainNodes;
	TMap<FVoxelPublishGroupKey, TArray<FVoxelViewKey>> VisibleTerrainNodesByGroup;
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
