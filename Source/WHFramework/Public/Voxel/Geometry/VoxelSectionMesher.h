#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Geometry/VoxelMeshAttributeStream.h"
struct FVoxelBoundaryTransitionContext;
struct FVoxelFrozenHeightfieldInputs;
struct FVoxelVoxelProxyData;
struct FVoxelSurfaceTileData;
struct FVoxelMacroTileData;
struct FVoxelWaterSurfaceTileData;
struct WHFRAMEWORK_API FVoxelMeshBuffers
{
    TArray<FVector> Vertices;
    TVoxelMeshAttributeStream<FVector> Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV0;
    TVoxelMeshAttributeStream<FVector2D> UV1,UV2;
    TVoxelMeshAttributeStream<FLinearColor> Colors;
    TVoxelMeshAttributeStream<FProcMeshTangent> Tangents;
    int32 PlannedVertices = 0;
    int32 PlannedIndices = 0;
    int32 CapacityPlanIndex = INDEX_NONE;
    uint64 Bytes()const;
    uint64 AllocatedBytes() const;
    uint64 PlannedBytes() const;
    void ReservePlan(const FVoxelMeshBuffers& InPlan);
    bool Validate()const;
    bool CompactAttributes(uint64 InWorkingBytes, const TAtomic<bool>* InCancel = nullptr);
};
struct WHFRAMEWORK_API FVoxelRenderBatch
{
	EVoxelRenderGroup Group = EVoxelRenderGroup::Opaque;
	uint16 Bank = 0;
	/** Optional purely visual variant within a material bank (zero is the base). */
	uint8 MaterialVariant = 0;
	FVoxelMeshBuffers Mesh;
};
struct WHFRAMEWORK_API FVoxelSectionMeshResult
{
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> InputWorkReceipt;
	TSharedPtr<const FVoxelBoundaryTransitionContext, ESPMode::ThreadSafe> FrozenBoundaryInputs;
	TSharedPtr<const FVoxelFrozenHeightfieldInputs, ESPMode::ThreadSafe> FrozenHeightfieldInputs;
	TSharedPtr<const FVoxelTaskCustomPayload, ESPMode::ThreadSafe> FrozenNativeInterfaceInputs;
	TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> FrozenVoxelProxy;
	TSharedPtr<const FVoxelSurfaceTileData, ESPMode::ThreadSafe> FrozenSurface;
	TSharedPtr<const FVoxelMacroTileData, ESPMode::ThreadSafe> FrozenMacro;
	TSharedPtr<const FVoxelWaterSurfaceTileData, ESPMode::ThreadSafe> FrozenWater;
    FVoxelTaskStamp Stamp;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
    TArray<FVoxelRenderBatch> Batches;
    uint64 Bytes()const;
    uint64 AllocatedBytes() const;
    uint64 PlannedBytes() const;
	bool ValidateAndCache();
	bool CompactAttributes(uint64 InWorkingBytes, const TAtomic<bool>* InCancel = nullptr);
	bool TryCompactAttributes(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	FBox LocalBounds = FBox(ForceInit);
	bool bValidated = false;
	uint64 VertexCount = 0;
	uint64 TriangleCount = 0;
	uint64 MeshDataBytes = 0;
};
class WHFRAMEWORK_API FVoxelSectionMesher
{
public:
	static uint64 FaceMaskBytes(int32 InSide);
	static uint64 TransitionClipScratchBytes(const FVoxelBoundaryTransitionContext* InTransition,
		const FIntVector& InLocalCellOffset, int32 InSide);
	static bool IsKnownEmpty(const FVoxelSectionSnapshot& InSnapshot, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelBoundaryTransitionContext* InTransition = nullptr);
    static bool Build(const FVoxelSectionSnapshot& Snapshot,const FVoxelRegistrySnapshot& Registry,
        const FVoxelShapeRegistry& Shapes,FVoxelSectionMeshResult& Out,const TAtomic<bool>* Cancel=nullptr,
        double InTextureRepeatsPerCell=1.0,
        const FVoxelBoundaryTransitionContext* InTransition=nullptr,
		bool bBoundaryOnly = false, bool bIncludeBoundary = true,
		const FIntVector& InLocalCellOffset = FIntVector::ZeroValue,
		int32 InTextureGridSide = 0, bool bCountOnly = false,
		const FVoxelSectionMeshResult* InCapacityPlan = nullptr);
};
