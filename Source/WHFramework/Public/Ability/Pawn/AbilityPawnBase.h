// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityPawnInterface.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Common/Targeting/TargetingAgentInterface.h"
#include "Pawn/Base/PawnBase.h"

#include "AbilityPawnBase.generated.h"

class UFSMComponent;
class UInteractionComponent;
class UVitalityAttributeSetBase;
class UVitalityAbilityBase;
class UAbilitySystemComponentBase;
class UBoxComponent;
class UAttributeSetBase;
class UAbilityPawnInventoryBase;
class UAbilityPawnDataBase;

/**
 * Ability Pawn基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPawnBase : public APawnBase, public IAbilityPawnInterface, public IFSMAgentInterface, public IInteractionAgentInterface, public IAbilityInventoryAgentInterface, public ITargetingAgentInterface
{
	GENERATED_BODY()

	friend class UAbilityPawnState_Death;
	friend class UAbilityPawnState_Spawn;

public:
	AAbilityPawnBase(const FObjectInitializer& ObjectInitializer);

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

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BindASCInput() override;

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

protected:
	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;
	
	virtual void ResetData() override;

	virtual void OnFiniteStateRefresh(UFiniteStateBase* InCurrentState) override;

public:
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

	virtual void OnAdditionItem(const FAbilityItem& InItem) override;

	virtual void OnRemoveItem(const FAbilityItem& InItem) override;

	virtual void OnPreChangeItem(const FAbilityItem& InOldItem) override;

	virtual void OnChangeItem(const FAbilityItem& InNewItem) override;

	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;
		
	virtual void OnDeactiveItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

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
	UAbilitySystemComponentBase* AbilitySystem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UVitalityAttributeSetBase* AttributeSet;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* Interaction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilityPawnInventoryBase* Inventory;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PawnStats")
	FName RaceID;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PawnStats")
	int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PawnStats")
	FTransform BirthTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PawnStats")
	float MovementRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "PawnStats")
	float RotationRate;

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
	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override;

	virtual UShapeComponent* GetCollisionComponent() const override;

	virtual UMeshComponent* GetMeshComponent() const override;

	template<class T>
	T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual IInteractionAgentInterface* GetInteractingAgent() const override { return IInteractionAgentInterface::GetInteractingAgent(); }

	template<class T>
	T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InClass) const { return GetDeterminesOutputObject(Cast<AActor>(GetInteractingAgent()), InClass); }

	virtual EInteractAgentType GetInteractAgentType() const override { return EInteractAgentType::Initiative; }

	virtual UInteractionComponent* GetInteractionComponent() const override;
	
	virtual UAbilityInventoryBase* GetInventory() const override;

	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsPlayer() const override;

	virtual bool IsEnemy(IAbilityPawnInterface* InTarget) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsActive(bool bNeedNotDead = false) const override;
	
	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = true) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsDying() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsWalking(bool bReally = false) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsInterrupting() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsMoving() const override;
	
	virtual bool IsTargetAble_Implementation(APawn* InPlayerPawn) const override;

public:
	UFUNCTION(BlueprintPure)
	virtual FName GetNameA() const override { return Name; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNameA(FName InName) override { Name = InName; }
	
	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override { RaceID = InRaceID; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelA() const override { return Level; }

	UFUNCTION(BlueprintCallable)
	virtual bool SetLevelA(int32 InLevel) override;

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetRadius() const override;

	UFUNCTION(BlueprintPure)
	virtual float GetHalfHeight() const override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetDistance(AActor* InTargetActor, bool bIgnoreRadius = true, bool bIgnoreZAxis = true) const override;

	UFUNCTION(BlueprintPure)
	virtual FTransform GetBirthTransform() const override { return BirthTransform; }

	UFUNCTION(BlueprintPure)
	virtual void GetMotionRate(float& OutMovementRate, float& OutRotationRate) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void SetMotionRate(float InMovementRate, float InRotationRate) override;
	
	UFUNCTION(BlueprintPure)
	virtual bool HasActionAbility(const FGameplayTag& InActionTag) const override;

	UFUNCTION(BlueprintPure)
	virtual FVitalityActionAbilityData GetActionAbility(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintPure)
	virtual TMap<FGameplayTag, FVitalityActionAbilityData>& GetActionAbilities() override { return ActionAbilities; }

public:
	virtual void OnRep_Controller() override;

	virtual void OnRep_PlayerState() override;
};
