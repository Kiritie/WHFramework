// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHActor.h"
#include "GameFramework/Actor.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelChunk.generated.h"

class IVoxelAgentInterface;
class UVoxelModule;
class ACharacterBase;
class UVoxel;
class AAbilityPickUpBase;
class AAbilityVitalityBase;
class UVoxelMeshComponent;

/**
 * 体素块
 */
UCLASS()
class WHFRAMEWORK_API AVoxelChunk : public AWHActor, public ISceneContainerInterface
{
	GENERATED_BODY()

	friend class UVoxelModule;

public:	
	// Sets default values for this actor's properties
	AVoxelChunk();

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	//////////////////////////////////////////////////////////////////////////
	// Chunk
public:
	virtual void Initialize(UVoxelModule* InModule, FIndex InIndex, int32 InBatch);

	virtual void Generate(EPhase InPhase);

	virtual void CreateMesh();

	virtual void BuildMap(int32 InStage);

	virtual void BuildMesh();

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
	virtual bool HasVoxel(FIndex InIndex, bool bSafe = false);

	virtual bool HasVoxel(int32 InX, int32 InY, int32 InZ, bool bSafe = false);

	virtual bool HasVoxelComplex(FIndex InIndex, bool bSafe = false);

	virtual bool HasVoxelComplex(int32 InX, int32 InY, int32 InZ, bool bSafe = false);

	virtual FVoxelItem& GetVoxel(FIndex InIndex, bool bMainPart = false);

	virtual FVoxelItem& GetVoxel(int32 InX, int32 InY, int32 InZ, bool bMainPart = false);

	virtual FVoxelItem& GetVoxelComplex(FIndex InIndex, bool bMainPart = false);

	virtual FVoxelItem& GetVoxelComplex(int32 InX, int32 InY, int32 InZ, bool bMainPart = false);

public:
	virtual bool CheckVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, FVector InRange = FVector::OneVector);

	virtual bool CheckVoxelAdjacent(FIndex InIndex, EDirection InDirection);

	virtual bool CheckVoxelAdjacent(const FVoxelItem& InVoxelItem, EDirection InDirection);

	virtual bool CheckVoxelNeighbors(FIndex InIndex, EVoxelType InVoxelType, FVector InRange = FVector::OneVector, bool bFromCenter = false, bool bIgnoreBottom = false, bool bOnTheChunk = false);

	virtual void SetVoxel(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bSafe = false);

	virtual void SetVoxel(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bSafe = false);

	virtual bool SetVoxelSample(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelSample(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelComplex(FIndex InIndex, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelComplex(int32 InX, int32 InY, int32 InZ, const FVoxelItem& InVoxelItem, bool bGenerate = false, IVoxelAgentInterface* InAgent = nullptr);

	virtual bool SetVoxelComplex(const TMap<FIndex, FVoxelItem>& InVoxelItems, bool bGenerate = false, bool bFirstSample = false, IVoxelAgentInterface* InAgent = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// Topography
public:
	virtual FVoxelTopography& GetTopography(FIndex InIndex);

	virtual FVoxelTopography& GetTopography(int32 InX, int32 InY, int32 InZ);

	virtual void SetTopography(FIndex InIndex, const FVoxelTopography& InTopography);

	virtual void SetTopography(int32 InX, int32 InY, int32 InZ, const FVoxelTopography& InTopography);

	//////////////////////////////////////////////////////////////////////////
	// SceneActor
protected:
	UPROPERTY(VisibleAnywhere, Category = "SceneActor")
	TMap<FGuid, AActor*> SceneActorMap;

public:
	virtual void SetActorVisible_Implementation(bool bInVisible) override;
	
	virtual bool HasSceneActor(const FString& InID, bool bEnsured) const override;

	template<class T>
	T* GetSceneActor(const FString& InID, bool bEnsured = true) const
	{
		return Cast<T>(GetSceneActor(InID, nullptr, bEnsured));
	}

	virtual AActor* GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured) const override;
	
	virtual bool AddSceneActor(AActor* InActor) override;

	virtual bool RemoveSceneActor(AActor* InActor) override;

	virtual void GenerateSceneActors();

	virtual void LoadSceneActors(FSaveData* InSaveData) override;

	virtual void SpawnSceneActors() override;

	virtual void DestroySceneActors() override;

	//////////////////////////////////////////////////////////////////////////
	// Auxiliary
public:
	virtual AVoxelAuxiliary* SpawnAuxiliary(FVoxelItem& InVoxelItem);

	virtual void DestroyAuxiliary(FVoxelItem& InVoxelItem);
	
	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TMap<EVoxelNature, UVoxelMeshComponent*> MeshComponents;

	TArray<EVoxelNature> MeshVoxelNatures;

public:
	void SpawnMeshComponents(int32 InStage = 1 | 2);

	void DestroyMeshComponents();

	UVoxelMeshComponent* GetMeshComponent(EVoxelNature InVoxelNature);

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

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	UVoxelModule* Module;

	TMap<FIndex, FVoxelItem> VoxelMap;

	TMap<FIndex, FVoxelTopography> TopographyMap;

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

	FIndex GetWorldIndex() const;

	FVector GetWorldLocation() const;
};
