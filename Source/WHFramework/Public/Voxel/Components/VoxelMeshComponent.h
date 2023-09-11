// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelMeshComponent.generated.h"

/**
 * 体素网格组件
 */
UCLASS(Blueprintable, ClassGroup = (Rendering, Common), hidecategories = Object, config = Engine, editinlinenew, meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UVoxelMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()
	
public:
	UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector OffsetScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector CenterOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EVoxelMeshNature MeshNature;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EVoxelTransparency Transparency;

private:
	TArray<FVector> Vertices;

	TArray<int32> Triangles;

	TArray<FVector> Normals;

	TArray<FVector2D> UVs;

	TArray<FColor> VertexColors;

	TArray<FProcMeshTangent> Tangents;

public:
	void Initialize(EVoxelMeshNature InMeshNature, EVoxelTransparency InTransparency = EVoxelTransparency::Solid);
	
	void BuildVoxel(const FVoxelItem& InVoxelItem);
	
	void CreateVoxel(const FVoxelItem& InVoxelItem);

	void CreateMesh(int32 InSectionIndex = 0, bool bHasCollision = true);

	void ClearMesh(int32 InSectionIndex = 0);

	void ClearData();

private:
	void BuildFace(const FVoxelItem& InVoxelItem, EDirection InFacing);

	void BuildFace(const FVoxelItem& InVoxelItem, FVector InVertices[4], int32 InFaceIndex, FVector InNormal);

public:
	bool HasData() const;

	bool HasMesh();

	AVoxelChunk* GetOwnerChunk() const;

	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType) override;

	virtual void SetCollisionEnabled(bool bEnable);
};
