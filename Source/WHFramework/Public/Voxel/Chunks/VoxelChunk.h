// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelChunk.generated.h"

class ACharacterBase;
class UVoxel;
class APickUp;
class AAbilityVitalityBase;
class UVoxelMeshComponent;

/**
 * 体素块
 */
UCLASS()
class WHFRAMEWORK_API AVoxelChunk : public AActor, public IObjectPoolInterface, public ISceneContainerInterface, public ISaveDataInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelChunk();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void OnSpawn_Implementation() override;
		
	virtual void OnDespawn_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* SolidMesh;
	
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* SemiMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UVoxelMeshComponent* TransMesh;

public:
	UVoxelMeshComponent* GetSolidMesh();

	UVoxelMeshComponent* GetSemiMesh();

	UVoxelMeshComponent* GetTransMesh();
	
public:
	void DestroySolidMesh();

	void DestroySemiMesh();

	void DestroyTransMesh();

	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	TArray<AVoxelChunk*> Neighbors;
	
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	TArray<APickUp*> PickUps;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	FIndex Index;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Batch;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	bool bGenerated;

	TMap<FIndex, FVoxelItem> VoxelMap;
	
public:
	FIndex GetIndex() const { return Index; }

	int32 GetBatch() const { return Batch; }
	
	bool IsGenerated() const { return bGenerated; }

	TArray<AVoxelChunk*> GetNeighbors() const { return Neighbors; }

	//////////////////////////////////////////////////////////////////////////
	// Chunk
public:
	virtual void Initialize(FIndex InIndex, int32 InBatch);

	virtual void LoadData(FSaveData* InSaveData) override;

	virtual FSaveData* ToData(bool bSaved = true) override;

	virtual void Generate(bool bPreview = false);

	virtual void BuildMap();

	virtual void GenerateMap();

	virtual void SpawnActors();

	virtual void DestroyActors();

protected:
	virtual void OnGenerated(bool bPreview = false);

	void GenerateNeighbors(FIndex InIndex);

	void GenerateNeighbors(int32 InX, int32 InY, int32 InZ);

	void UpdateNeighbors();

	void BreakNeighbors();

public:
	bool IsOnTheChunk(FIndex InIndex) const;

	bool IsOnTheChunk(FVector InLocation) const;

	FIndex LocationToIndex(FVector InLocation, bool bWorldSpace = true) const;

	FVector IndexToLocation(FIndex InIndex, bool bWorldSpace = true) const;

	FIndex LocalIndexToWorld(FIndex InIndex) const;

	FIndex WorldIndexToLocal(FIndex InIndex) const;

	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	UVoxel* GetVoxel(FIndex InIndex);

	UVoxel* GetVoxel(int32 InX, int32 InY, int32 InZ);

	FVoxelItem& GetVoxelItem(FIndex InIndex);

	FVoxelItem& GetVoxelItem(int32 InX, int32 InY, int32 InZ);

public:
	bool CheckVoxel(FIndex InIndex, FVector InRange = FVector::OneVector, bool bIgnoreTransparent = true);

	bool CheckVoxel(int32 InX, int32 InY, int32 InZ, FVector InRange = FVector::OneVector, bool bIgnoreTransparent = true);

	bool CheckAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection);

	bool CheckNeighbors(const FVoxelItem& InVoxelItem, EVoxelType InVoxelType, bool bIgnoreBottom = false, int32 InDistance = 1);

	bool SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	bool SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	bool SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	bool SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerateMesh = false);

	bool GenerateVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem);

	bool DestroyVoxel(FIndex InIndex);

	bool DestroyVoxel(const FVoxelItem& InVoxelItem);

	bool ReplaceVoxel(const FVoxelItem& InOldVoxelItem, const FVoxelItem& InNewVoxelItem);

	//////////////////////////////////////////////////////////////////////////
	// Auxiliary
public:
	AVoxelAuxiliary* SpawnAuxiliary(FVoxelItem& InVoxelItem);

	void DestroyAuxiliary(AVoxelAuxiliary* InAuxiliary);

	//////////////////////////////////////////////////////////////////////////
	// SceneContainer
public:
	virtual void AddSceneActor(AActor* InActor) override;

	virtual void RemoveSceneActor(AActor* InActor) override;

	virtual void DestroySceneActor(AActor* InActor) override;

	//////////////////////////////////////////////////////////////////////////
	// PickUp
public:
	APickUp* SpawnPickUp(FAbilityItem InItem, FVector InLocation);

	APickUp* SpawnPickUp(FPickUpSaveData InPickUpData);

	void AttachPickUp(APickUp* InPickUp);

	void DetachPickUp(APickUp* InPickUp);

	void DestroyPickUp(APickUp* InPickUp);
};

