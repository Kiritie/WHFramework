// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityVitalityInterface.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "AbilityVitalityBase.generated.h"

class UFSMComponent;
class UInteractionComponent;
class UVitalityAttributeSetBase;
class UVitalityAbilityBase;
class UAbilitySystemComponentBase;
class UBoxComponent;
class UAttributeSetBase;
class UAbilityVitalityInventoryBase;

/**
 * Ability Vitality基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityVitalityBase : public AAbilityActorBase, public IAbilityVitalityInterface, public IFSMAgentInterface, public IVoxelAgentInterface
{
	GENERATED_UCLASS_BODY()

	friend class UAbilityVitalityState_Death;
	friend class UAbilityVitalityState_Default;

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }
	
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;
	
	virtual void ResetData() override;

	virtual void OnFiniteStateChanged(UFiniteStateBase* InFiniteState) override;

public:
	virtual void Serialize(FArchive& Ar) override;

	virtual void Death(IAbilityVitalityInterface* InKiller = nullptr) override;

	virtual void Kill(IAbilityVitalityInterface* InTarget) override;

	virtual void Revive(IAbilityVitalityInterface* InRescuer = nullptr) override;
	
	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity) override;

	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;
		
	virtual void OnCancelItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnAssembleItem(const FAbilityItem& InItem) override;

	virtual void OnDischargeItem(const FAbilityItem& InItem) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

public:
	virtual bool OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult) override;

	virtual bool OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName RaceID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FPrimaryAssetId GenerateVoxelID;

public:
	template<class T>
	T& GetVitalityData() const
	{
		return static_cast<T&>(GetVitalityData());
	}
	
	UAbilityVitalityDataBase& GetVitalityData() const;

	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = true) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsDying() const override;

public:
	virtual int32 GetLevelV() const override { return Level; }

	virtual bool SetLevelV(int32 InLevel) override;
	
	virtual float GetRadius() const override { return Super::GetRadius(); }

	virtual float GetHalfHeight() const override { return Super::GetRadius(); }
	
	UFUNCTION(BlueprintPure)
	virtual FName GetNameV() const override { return Name; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNameV(FName InName) override { Name = InName; }
	
	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override { RaceID = InRaceID; }

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override { return Super::GetAttributeSet(); }

	template<class T>
	T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return Super::GetAbilitySystemComponent(); }

public:
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
