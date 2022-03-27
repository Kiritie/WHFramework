// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ObjectPool/ObjectPoolInterface.h"
#include "Voxel/VoxelModuleTypes.h"

#include "Voxel.generated.h"

class ACharacterBase;
class AVoxelChunk;
class UAbilityCharacterPart;
class UVoxelAssetBase;

/**
 * ����
 */
UCLASS()
class WHFRAMEWORK_API UVoxel : public UObject, public IObjectPoolInterface
{
	GENERATED_BODY()

public:
	UVoxel();
	
	//////////////////////////////////////////////////////////////////////////
	// Statics
public:
	static UVoxel* EmptyVoxel;

	static UVoxel* UnknownVoxel;

public:
	static UVoxel* SpawnVoxel(EVoxelType InVoxelType);

	static UVoxel* SpawnVoxel(const FPrimaryAssetId& InVoxelID);

	static UVoxel* LoadVoxel(AVoxelChunk* InOwner, const FVoxelItem& InVoxelItem);

	static UVoxel* LoadVoxel(AVoxelChunk* InOwner, const FString& InVoxelData);
	
	static void DespawnVoxel(UVoxel* InVoxel);

	static bool IsValid(UVoxel* InVoxel);

	//////////////////////////////////////////////////////////////////////////
	// Defaults
public:
	virtual void LoadData(const FString& InValue);

	virtual void LoadItem(const FVoxelItem& InVoxelItem);

	virtual FString ToData();

	virtual FVoxelItem ToItem();

	//////////////////////////////////////////////////////////////////////////
	// Events
public:
	virtual void OnSpawn_Implementation() override;

	virtual void OnDespawn_Implementation() override;

	virtual void OnTargetHit(ACharacterBase* InTarget, const FVoxelHitResult& InHitResult);

	virtual void OnTargetEnter(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult);

	virtual void OnTargetStay(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult);

	virtual void OnTargetExit(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult);

	virtual bool OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult);

	virtual bool OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult);

	virtual bool OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult);

	virtual void OnMouseHover(const FVoxelHitResult& InHitResult);

	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(BlueprintReadOnly)
	FPrimaryAssetId ID;

	UPROPERTY(BlueprintReadWrite)
	FIndex Index;

	UPROPERTY(BlueprintReadWrite)
	FRotator Rotation;
	
	UPROPERTY(BlueprintReadWrite)
	FVector Scale;
	
	UPROPERTY(BlueprintReadWrite)
	TMap<FName, FParameter> Params;

	UPROPERTY(BlueprintReadWrite)
	AVoxelChunk* Owner;

	UPROPERTY(BlueprintReadWrite)
	AVoxelAuxiliary* Auxiliary;

public:
	UFUNCTION(BlueprintPure)
	UVoxelAssetBase& GetData() const;

	UFUNCTION(BlueprintPure)
	FPrimaryAssetId GetID() const { return ID; }

	UFUNCTION(BlueprintCallable)
	void SetID(const FPrimaryAssetId& val) { ID = val; }

	UFUNCTION(BlueprintPure)
	FIndex GetIndex() const { return Index; }

	UFUNCTION(BlueprintCallable)
	void SetIndex(FIndex val) { Index = val; }

	UFUNCTION(BlueprintPure)
	FRotator GetRotation() const { return Rotation; }

	UFUNCTION(BlueprintCallable)
	void SetRotation(FRotator val) { Rotation = val; }

	UFUNCTION(BlueprintPure)
	FVector GetScale() const { return Scale; }
	
	UFUNCTION(BlueprintCallable)
	void SetScale(FVector val) { Scale = val; }

	UFUNCTION(BlueprintPure)
	AVoxelChunk* GetOwner() const { return Owner; }

	UFUNCTION(BlueprintCallable)
	void SetOwner(AVoxelChunk* val) { Owner = val; }

	UFUNCTION(BlueprintPure)
	AVoxelAuxiliary* GetAuxiliary() const { return Auxiliary; }

	UFUNCTION(BlueprintCallable)
	void SetAuxiliary(AVoxelAuxiliary* val) { Auxiliary = val; }
};
