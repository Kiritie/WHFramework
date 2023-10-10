// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelChunk.generated.h"

class IVoxelAgentInterface;
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

	friend class AVoxelModule;

public:	
	// Sets default values for this actor's properties
	AVoxelChunk();

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	UFUNCTION()
	virtual void OnCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	virtual int32 GetLimit_Implementation() const override { return 5000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void SetActorVisible_Implementation(bool bNewVisible) override;

	//////////////////////////////////////////////////////////////////////////
	// Chunk
public:
	virtual void Initialize(FIndex InIndex, int32 InBatch);

	virtual void Generate(EPhase InPhase);

	virtual void UnGenerate(EPhase InPhase);

	virtual void CreateMesh();

	virtual void BuildMap(int32 InStage);

	virtual void BuildMesh();

	virtual void SpawnActors();

	virtual void LoadActors(FSaveData* InSaveData);

	virtual void CreateActors();

	virtual void DestroyActors();

protected:
	virtual void GenerateNeighbors(FIndex InIndex, EPhase InPhase = EPhase::Primary);

	virtual void GenerateNeighbors(int32 InX, int32 InY, int32 InZ, EPhase InPhase = EPhase::Primary);

	virtual void UpdateNeighbors();

	virtual void BreakNeighbors();

public:
	virtual bool IsOnTheChunk(FIndex InIndex) const;

	virtual bool IsOnTheChunk(FVector InLocation) const;

	virtual FIndex LocationToIndex(FVector InLocation, bool bWorldSpace = true) const;

	virtual FVector IndexToLocation(FIndex InIndex, bool bWorldSpace = true) const;

	virtual FIndex LocalIndexToWorld(FIndex InIndex) const;

	virtual FIndex WorldIndexToLocal(FIndex InIndex) const;

	virtual bool LocalIndexToNeighbor(FIndex InIndex, EDirection& OutDirection) const;

	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	virtual bool HasVoxel(FIndex InIndex);

	virtual bool HasVoxel(int32 InX, int32 InY, int32 InZ);

	template<class T>
	T& GetVoxel(FIndex InIndex, bool bMainPart = false)
	{
		return static_cast<T&>(GetVoxel(InIndex, bMainPart));
	}
	virtual UVoxel& GetVoxel(FIndex InIndex, bool bMainPart = false);

	virtual FVoxelItem& GetVoxelItem(FIndex InIndex, bool bMainPart = false);

	virtual FVoxelItem& GetVoxelItem(int32 InX, int32 InY, int32 InZ, bool bMainPart = false);

public:
	virtual bool CheckVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, FVector InRange = FVector::OneVector);

	virtual bool CheckVoxelAdjacent(FIndex InIndex, EDirection InDirection);

	virtual bool CheckVoxelAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection);

	virtual bool CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange = FVector::OneVector, bool bFromCenter = false, bool bIgnoreBottom = false, bool bOnTheChunk = false);

	virtual bool SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelComplex(const TMap<FIndex, FVoxelItem>& InVoxelItems, bool bGenerate = false, bool bFirstSample = false, IVoxelAgentInterface* InAgent = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// SceneContainer
public:
	virtual void AddSceneActor(AActor* InActor) override;

	virtual void RemoveSceneActor(AActor* InActor) override;

	//////////////////////////////////////////////////////////////////////////
	// Auxiliary
public:
	virtual AVoxelAuxiliary* SpawnAuxiliary(FVoxelItem& InVoxelItem);

	virtual void DestroyAuxiliary(FVoxelItem& InVoxelItem);
	
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
	FIndex Index;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Batch;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	bool bBuilded;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	bool bGenerated;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	bool bChanged;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	TMap<EDirection, AVoxelChunk*> Neighbors;
	
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	TArray<AAbilityPickUpBase*> PickUps;

	TMap<FIndex, FVoxelItem> VoxelMap;
	
public:
	FIndex GetIndex() const { return Index; }

	int32 GetBatch() const { return Batch; }

	bool IsBuilded() const { return bBuilded; }

	bool IsGenerated() const { return bGenerated; }
	
	bool IsChanged() const { return bChanged; }
	
	void SetChanged(bool bInChanged) { bChanged = bInChanged; }

	AVoxelChunk* GetNeighbor(EDirection InDirection) const { return Neighbors[InDirection]; }

	AVoxelChunk* GetOrSpawnNeighbor(EDirection InDirection, bool bAddToQueue = true);

	TMap<EDirection, AVoxelChunk*> GetNeighbors() const { return Neighbors; }

	FVector GetChunkLocation() const;
};

