// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelChunk.generated.h"

class AVoxelModule;
class ACharacterBase;
class UVoxel;
class AAbilityPickUpBase;
class AAbilityVitalityBase;
class UVoxelMeshComponent;

/**
 * 体素块
 */
UCLASS()
class WHFRAMEWORK_API AVoxelChunk : public AWHActor, public ISceneContainerInterface, public ISaveDataInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelChunk();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

	UFUNCTION()
	virtual void OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// Chunk
public:
	virtual void Initialize(AVoxelModule* InModule,  FIndex InIndex, int32 InBatch);

	virtual void Generate(bool bForceMode = false);

	virtual void BuildMap(int32 InStage);

	virtual void GenerateMap();

	virtual void SpawnActors();

	virtual void DestroyActors();

protected:
	virtual void GenerateNeighbors(FIndex InIndex, bool bForceMode = false);

	virtual void GenerateNeighbors(int32 InX, int32 InY, int32 InZ, bool bForceMode = false);

	virtual void UpdateNeighbors();

	virtual void BreakNeighbors();

public:
	virtual bool IsOnTheChunk(FIndex InIndex) const;

	virtual bool IsOnTheChunk(FVector InLocation) const;

	virtual FIndex LocationToIndex(FVector InLocation, bool bWorldSpace = true) const;

	virtual FVector IndexToLocation(FIndex InIndex, bool bWorldSpace = true) const;

	virtual FIndex LocalIndexToWorld(FIndex InIndex) const;

	virtual FIndex WorldIndexToLocal(FIndex InIndex) const;

	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	virtual UVoxel& GetVoxel(FIndex InIndex);

	virtual UVoxel& GetVoxel(int32 InX, int32 InY, int32 InZ);

	virtual FVoxelItem& GetVoxelItem(FIndex InIndex);

	virtual FVoxelItem& GetVoxelItem(int32 InX, int32 InY, int32 InZ);

public:
	virtual bool CheckVoxel(FIndex InIndex, FVector InRange = FVector::OneVector, bool bIgnoreTransparent = true);

	virtual bool CheckVoxel(int32 InX, int32 InY, int32 InZ, FVector InRange = FVector::OneVector, bool bIgnoreTransparent = true);

	virtual bool CheckAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection);

	virtual bool CheckNeighbors(const FVoxelItem& InVoxelItem, EVoxelType InVoxelType, bool bIgnoreBottom = false, int32 InDistance = 1);

	virtual bool SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	virtual bool SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	virtual bool SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	virtual bool SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	virtual bool GenerateVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem);

	virtual bool DestroyVoxel(FIndex InIndex);

	virtual bool DestroyVoxel(const FVoxelItem& InVoxelItem);

	virtual bool ReplaceVoxel(const FVoxelItem& InOldVoxelItem, const FVoxelItem& InNewVoxelItem);

	//////////////////////////////////////////////////////////////////////////
	// SceneContainer
public:
	virtual void AddSceneActor(AActor* InActor) override;

	virtual void RemoveSceneActor(AActor* InActor) override;

	//////////////////////////////////////////////////////////////////////////
	// Auxiliary
public:
	virtual AVoxelAuxiliary* SpawnAuxiliary(FVoxelItem& InVoxelItem);

	virtual void DestroyAuxiliary(AVoxelAuxiliary* InAuxiliary);

	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* SolidMesh;
	
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* SemiMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* TransMesh;

	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	TArray<AVoxelChunk*> Neighbors;
	
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	TArray<AAbilityPickUpBase*> PickUps;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	AVoxelModule* Module;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FIndex Index;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Batch;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	bool bGenerated;

	TMap<FIndex, FVoxelItem> VoxelMap;
	
public:
	virtual bool IsEntity() const;

	AVoxelModule* GetModule() const { return Module; }

	FIndex GetIndex() const { return Index; }

	int32 GetBatch() const { return Batch; }

	bool IsGenerated() const { return bGenerated; }

	TArray<AVoxelChunk*> GetNeighbors() const { return Neighbors; }
};

