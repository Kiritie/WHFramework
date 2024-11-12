// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/Attributes/CharacterAttributeSetBase.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "Ability/PickUp/AbilityPickerInterface.h"
#include "Character/Base/CharacterBase.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Character/AbilityCharacterInterface.h"
#include "Common/Targeting/TargetingAgentInterface.h"

#include "AbilityCharacterBase.generated.h"

class UFSMComponent;
class UInteractionComponent;
class UCharacterAttributeSetBase;
class UBoxComponent;
class AVoxelChunk;
class UVoxel;
class AController;
class UAbilityBase;
class UAbilitySystemComponentBase;
class AAbilityProjectileBase;
class UAbilityCharacterInventoryBase;

/**
 * Ability Character基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityCharacterBase : public ACharacterBase, public IAbilityCharacterInterface, public IFSMAgentInterface, public IAbilityPickerInterface, public IInteractionAgentInterface, public IAbilityInventoryAgentInterface, public ITargetingAgentInterface
{
	GENERATED_BODY()

	friend class UAbilityCharacterState_Death;
	friend class UAbilityCharacterState_Spawn;
	friend class UAbilityCharacterState_Fall;
	friend class UAbilityCharacterState_Jump;
	friend class UAbilityCharacterState_Static;
	friend class UAbilityCharacterState_Walk;

public:
	AAbilityCharacterBase(const FObjectInitializer& ObjectInitializer);

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

	virtual void BindASCInput();

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

protected:
	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void ResetData() override;

	virtual void OnFiniteStateRefresh(UFiniteStateBase* InCurrentState) override;

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

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
	virtual void FreeToAnim() override;

	UFUNCTION(BlueprintCallable)
	virtual void LimitToAnim() override;

	virtual void Jump() override;

	UFUNCTION(BlueprintCallable)
	virtual void UnJump();

	virtual void Crouch(bool bClientSimulation) override;

	virtual void UnCrouch(bool bClientSimulation) override;

	UFUNCTION(BlueprintCallable)
	virtual void Swim() override;
		
	UFUNCTION(BlueprintCallable)
	virtual void UnSwim() override;
						
	UFUNCTION(BlueprintCallable)
	virtual void Float(float InWaterPosZ) override;
						
	UFUNCTION(BlueprintCallable)
	virtual void UnFloat() override;

	UFUNCTION(BlueprintCallable)
	virtual void Climb() override;
			
	UFUNCTION(BlueprintCallable)
	virtual void UnClimb() override;

	UFUNCTION(BlueprintCallable)
	virtual void Fly() override;

	UFUNCTION(BlueprintCallable)
	virtual void UnFly() override;

	UFUNCTION(BlueprintCallable)
	virtual bool DoAction(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintCallable)
	virtual bool StopAction(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintCallable)
	virtual void EndAction(const FGameplayTag& InActionTag, bool bWasCancelled) override;

public:
	virtual bool OnPickUp(AAbilityPickUpBase* InPickUp) override;

	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive) override;

	virtual void OnAdditionItem(const FAbilityItem& InItem) override;

	virtual void OnRemoveItem(const FAbilityItem& InItem) override;

	virtual void OnChangeItem(const FAbilityItem& InNewItem, FAbilityItem& InOldItem) override;
	
	virtual void OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess) override;
		
	virtual void OnDeactiveItem(const FAbilityItem& InItem, bool bPassive) override;

	virtual void OnDiscardItem(const FAbilityItem& InItem, bool bInPlace) override;

	virtual void OnSelectItem(const FAbilityItem& InItem) override;

	virtual void OnAuxiliaryItem(const FAbilityItem& InItem) override;

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;
	
	virtual void HandleDamage(const FGameplayAttribute& DamageAttribute, float DamageValue, float DefendValue, bool bHasCrited, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

	virtual void HandleRecovery(const FGameplayAttribute& RecoveryAttribute, float RecoveryValue, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

	virtual void HandleInterrupt(const FGameplayAttribute& InterruptAttribute, float InterruptDuration, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

protected:
	// stats
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FName RaceID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FTransform BirthTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	float MovementRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	float RotationRate;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterAttributeSetBase* AttributeSet;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* Interaction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilityCharacterInventoryBase* Inventory;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	bool bASCInputBound;

	float DefaultGravityScale;

	float DefaultAirControl;

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

	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, MoveSpeed)

	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, RotationSpeed)
	
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, SwimSpeed)
		
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, FlySpeed)

	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, JumpForce)

public:
	template<class T>
	T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override;

	virtual UShapeComponent* GetCollisionComponent() const override;

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
	
	UFUNCTION(BlueprintPure)
	virtual bool IsFreeToAnim() const override;
	
	UFUNCTION(BlueprintPure)
	virtual bool IsAnimating() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsJumping() const;

	UFUNCTION(BlueprintPure)
	virtual bool IsFalling(bool bReally = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool IsCrouching(bool bReally = false) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsSwimming(bool bReally = false) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsFloating() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsClimbing() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsFlying(bool bReally = false) const override;

	virtual bool IsTargetAble_Implementation(APawn* InPlayerPawn) const override;

	virtual bool IsLookAtAble_Implementation(AActor* InLookerActor) const override;

	virtual bool CanLookAtTarget() override;

public:
	UFUNCTION(BlueprintPure)
	virtual FName GetNameA() const override { return Name; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNameA(FName InName) override;

	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override;

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
	virtual float GetDefaultGravityScale() const override { return DefaultGravityScale; }

	UFUNCTION(BlueprintPure)
	virtual float GetDefaultAirControl() const override { return DefaultAirControl; }
	
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
