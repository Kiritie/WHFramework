// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelMeshComponent.generated.h"

/**
 * 体素网格组件
 */
UCLASS(Blueprintable, ClassGroup = (Rendering, Common), hidecategories = Object, config = Engine, editinlinenew, meta = (BlueprintSpawnableComponent))
class WHFRAMEWORK_API UVoxelMeshComponent : public UProceduralMeshComponent, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:
	UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer);
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Initialize(EVoxelScope InScope, EVoxelNature InNature = EVoxelNature::None);

	virtual void Register(UObject* InOuter);
	
	virtual void UnRegister();

	virtual void BuildVoxel(const FVoxelItem& InVoxelItem);
	
	virtual void CreateVoxel(const FVoxelItem& InVoxelItem);

	virtual void CreateMesh(int32 InSectionIndex = 0, bool bHasCollision = true);

	virtual void ClearMesh(int32 InSectionIndex = 0);

	virtual void ClearData();

	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType) override;

	virtual void SetCollisionEnabled(bool bEnable);

protected:
	virtual void BuildFace(const FVoxelItem& InVoxelItem, EDirection InFacing);

	virtual void BuildFace(const FVoxelItem& InVoxelItem, FVector InVertices[4], int32 InFaceIndex, FVector InNormal);

protected:
	UFUNCTION()
	virtual void OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector OffsetScale;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector CenterOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EVoxelScope Scope;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	EVoxelNature Nature;

protected:
	TArray<FVector> Vertices;

	TArray<int32> Triangles;

	TArray<FVector> Normals;

	TArray<FVector2D> UVs;

	TArray<FColor> VertexColors;

	TArray<FProcMeshTangent> Tangents;

public:
	bool HasData() const;

	bool HasMesh();

	FVector GetOffsetScale() const { return OffsetScale; }

	FVector GetCenterOffset() const { return CenterOffset; }

	EVoxelScope GetScope() const { return Scope; }

	EVoxelNature GetNature() const { return Nature; }

	void SetNature(EVoxelNature InNature);

	AVoxelChunk* GetOwnerChunk() const;
};
