// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityVitalityInterface.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Ability/Interaction/InteractionAgentInterface.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "../Inventory/InventoryAgentInterface.h"

#include "AbilityVitalityBase.generated.h"

class UFSMComponent;
class UVitalityInteractionComponent;
class UVitalityAttributeSetBase;
class UVitalityAbilityBase;
class UAbilitySystemComponentBase;
class UBoxComponent;
class UAttributeSetBase;
class UVitalityInventory;

/**
 * Ability Vitality基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityVitalityBase : public AWHActor, public IAbilityVitalityInterface, public IFSMAgentInterface, public IVoxelAgentInterface, public IPrimaryEntityInterface, public IInteractionAgentInterface, public IInventoryAgentInterface, public ISaveDataInterface
{
	GENERATED_BODY()

	friend class UAbilityVitalityState_Death;
	friend class UAbilityVitalityState_Default;

public:
	// Sets default values for this actor's properties
	AAbilityVitalityBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UVitalityAttributeSetBase* AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UVitalityInteractionComponent* Interaction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UVitalityInventory* Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	// stats
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VitalityStats")
	FPrimaryAssetId AssetID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName RaceID;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FPrimaryAssetId GenerateVoxelID;

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }
	
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;
	
	virtual void ResetData();

	virtual void OnFiniteStateChanged(UFiniteStateBase* InFiniteState) override;

public:
	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;

	virtual void Death(IAbilityVitalityInterface* InKiller = nullptr) override;

	virtual void Kill(IAbilityVitalityInterface* InTarget) override;

	virtual void Revive(IAbilityVitalityInterface* InRescuer = nullptr) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;
	
	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;
		
	virtual void OnCancelItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnAssembleItem(const FAbilityItem& InItem) override;

	virtual void OnDischargeItem(const FAbilityItem& InItem) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

public:
	virtual bool GenerateVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;

public:
	template<class T>
	T& GetVitalityData() const
	{
		return static_cast<T&>(GetVitalityData());
	}
	
	UAbilityVitalityDataBase& GetVitalityData() const;

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override;

	template<class T>
	T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual UInteractionComponent* GetInteractionComponent() const override;
	
	virtual UInventory* GetInventory() const override;

	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	virtual FVector GetAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = true) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsDying() const override;

public:
	UFUNCTION(BlueprintPure)
	virtual FPrimaryAssetId GetAssetID() const override { return AssetID; }

	UFUNCTION(BlueprintPure)
	virtual FName GetNameV() const override { return Name; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNameV(FName InName) override { Name = InName; }
	
	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override { RaceID = InRaceID; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelV() const override { return Level; }

	UFUNCTION(BlueprintCallable)
	virtual bool SetLevelV(int32 InLevel) override;

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetRadius() const override;

	UFUNCTION(BlueprintPure)
	virtual float GetHalfHeight() const override;
			
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Exp)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxExp)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Health)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxHealth)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, PhysicsDamage)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MagicDamage)

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;
	
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;
};
