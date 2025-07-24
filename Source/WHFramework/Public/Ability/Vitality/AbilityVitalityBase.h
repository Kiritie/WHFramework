// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityVitalityInterface.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "AbilityVitalityBase.generated.h"

class UFSMComponent;
class UInteractionComponent;
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
	GENERATED_BODY()

	friend class UAbilityVitalityState_Death;
	friend class UAbilityVitalityState_Spawn;

public:
	AAbilityVitalityBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;
	
	virtual void ResetData() override;

	virtual void OnFiniteStateRefresh(UFiniteStateBase* InCurrentState) override;

public:
	virtual void Serialize(FArchive& Ar) override;

	virtual void Death(IAbilityVitalityInterface* InKiller) override;

	virtual void Kill(IAbilityVitalityInterface* InTarget) override;

	virtual void Revive(IAbilityVitalityInterface* InRescuer) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void Static() override;

	UFUNCTION(BlueprintCallable)
	virtual void UnStatic() override;

	UFUNCTION(BlueprintCallable)
	virtual void Interrupt(float InDuration = -1.f) override;

	UFUNCTION(BlueprintCallable)
	virtual void UnInterrupt() override;
	
	UFUNCTION(BlueprintCallable)
	virtual bool DoAction(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintCallable)
	virtual bool StopAction(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintCallable)
	virtual void EndAction(const FGameplayTag& InActionTag, bool bWasCancelled) override;

public:
	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive) override;

	virtual void OnPreChangeItem(const FAbilityItem& InOldItem) override;

	virtual void OnChangeItem(const FAbilityItem& InNewItem) override;

	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;

	virtual void OnRemoveItem(const FAbilityItem& InItem) override;
		
	virtual void OnDeactiveItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

protected:
	virtual bool OnGenerateVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult) override;

	virtual bool OnDestroyVoxel(EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult) override;

protected:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;

	virtual void OnActorAttached(AActor* InActor) override;

	virtual void OnActorDetached(AActor* InActor) override;

public:
	virtual void HandleDamage(const FGameplayAttribute& DamageAttribute, float DamageValue, float DefendValue, bool bHasCrited, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

	virtual void HandleRecovery(const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

	virtual void HandleInterrupt(const FGameplayAttribute& InterruptAttribute, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName RaceID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FPrimaryAssetId GenerateVoxelID;

	TMap<FGameplayTag, FVitalityActionAbilityData> ActionAbilities;

public:
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Exp)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxExp)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Health)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxHealth)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, HealthRecovery)
		
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, HealthRegenSpeed)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, PhysicsDamage)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MagicDamage)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, FallDamage)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Interrupt)

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsActive() const override;
	
	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = true) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsDying() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsWalking(bool bReally = false) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsInterrupting() const override;

public:
	virtual FName GetNameA() const override { return Super::GetNameA(); }
	
	virtual void SetNameA(FName InName) override { Super::SetNameA(InName); }
	
	virtual int32 GetLevelA() const override { return Super::GetLevelA(); }
	
	virtual bool SetLevelA(int32 InLevel) override { return Super::SetLevelA(InLevel); }
	
	virtual float GetRadius() const override { return Super::GetRadius(); }

	virtual float GetHalfHeight() const override { return Super::GetRadius(); }
		
	virtual float GetDistance(AActor* InTargetActor, bool bIgnoreRadius = true, bool bIgnoreZAxis = true) const override { return Super::GetDistance(InTargetActor, bIgnoreRadius, bIgnoreZAxis); }

	virtual FTransform GetBirthTransform() const override { return Super::GetBirthTransform(); }

	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override { RaceID = InRaceID; }

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;
	
	UFUNCTION(BlueprintPure)
	virtual bool HasActionAbility(const FGameplayTag& InActionTag) const override;

	UFUNCTION(BlueprintPure)
	virtual FVitalityActionAbilityData GetActionAbility(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintPure)
	virtual TMap<FGameplayTag, FVitalityActionAbilityData>& GetActionAbilities() override { return ActionAbilities; }

	template<class T>
	T& GetVitalityData() const
	{
		return static_cast<T&>(GetVitalityData());
	}
	
	UAbilityVitalityDataBase& GetVitalityData() const;

	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

	virtual FVector GetVoxelAgentLocation() const override { return GetActorLocation(); }

	virtual FPrimaryAssetId GetGenerateVoxelID() const override { return GenerateVoxelID; }

	virtual void SetGenerateVoxelID(const FPrimaryAssetId& InGenerateVoxelID) override { GenerateVoxelID = InGenerateVoxelID; }

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override { return Super::GetAttributeSet(); }

	virtual UShapeComponent* GetCollisionComponent() const override { return Super::GetCollisionComponent(); }

	virtual UMeshComponent* GetMeshComponent() const override { return Super::GetMeshComponent(); }

	template<class T>
	T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return Super::GetAbilitySystemComponent(); }
};
