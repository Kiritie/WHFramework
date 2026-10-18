// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ability/Attributes/CharacterAttributeSetBase.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "Character/Base/CharacterBase.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Character/AbilityCharacterInterface.h"
#include "Common/Targeting/TargetingAgentInterface.h"

#include "AbilityCharacterBase.generated.h"

class UFSMComponent;
class UInteractionComponent;
class UAbilityInventoryBase;
class UCharacterAttributeSetBase;
class UBoxComponent;
class UVoxelSceneRegion;
class UVoxel;
class AController;
class UAbilityBase;
class UAbilitySystemComponentBase;
class AAbilityProjectileBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FInventorySelectionChanged, const FInventorySlotKey&);

/**
 * Ability Character基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityCharacterBase : public ACharacterBase,
                                              public IAbilityCharacterInterface,
                                              public IFSMAgentInterface,
                                              public IInteractionAgentInterface,
                                              public IAbilityInventoryAgentInterface,
                                              public ITargetingAgentInterface
{
	GENERATED_BODY()

	friend class UAbilityCharacterState_Death;
	friend class UAbilityCharacterState_Spawn;

public:
	AAbilityCharacterBase(const FObjectInitializer& ObjectInitializer);
	void InitializeAbilities(AActor* InOwnerActor = nullptr, AActor* InAvatarActor = nullptr);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation() override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BindASCInput() override;

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

protected:
	virtual bool HasArchive() const override
	{
		return true;
	}

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;

	virtual FParameter ToData() override;

	virtual void ResetData() override;



public:
	virtual void Death(IAbilityVitalityInterface* InKiller) override;
	virtual bool IsAbilityRuntimeReady() const;
	bool IsBaseAttributeSourceReady() const { return bBaseAttributeSourceReady; }
	virtual bool IsMovementExternallyManaged() const;
	virtual void OnLifecycleActivated();
	virtual void OnLifecycleSuspended(bool bDeath);
	virtual void ApplyDeathAttributes();

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


public:


	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;


protected:
protected:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;

	virtual void OnActorAttached(AActor* InActor) override;

	virtual void OnActorDetached(AActor* InActor) override;

public:
	virtual void HandleDamage(const FGameplayAttribute& DamageAttribute,
	                          float DamageValue,
	                          float DefendValue,
	                          bool bHasCrited,
	                          const FHitResult& HitResult,
	                          const FGameplayTagContainer& SourceTags,
	                          AActor* SourceActor) override;

	virtual void HandleRecovery(const FGameplayAttribute& RecoveryAttribute,
	                            float RecoveryValue,
	                            const FHitResult& HitResult,
	                            const FGameplayTagContainer& SourceTags,
	                            AActor* SourceActor) override;

	virtual void HandleInterrupt(const FGameplayAttribute& InterruptAttribute,
	                             float InterruptDuration,
	                             const FHitResult& HitResult,
	                             const FGameplayTagContainer& SourceTags,
	                             AActor* SourceActor) override;

protected:
	// stats
	UPROPERTY(ReplicatedUsing = OnRep_Identity, VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FName RaceID;

	UPROPERTY(ReplicatedUsing = OnRep_Identity, VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 Level;

	UFUNCTION()
	virtual void OnRep_Identity();

	virtual void OnRep_Name() override;

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

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	UAbilityInventoryBase* Inventory;

	UPROPERTY(Transient, ReplicatedUsing=OnRep_SelectedInventorySlot)
	FInventorySlotKey SelectedInventorySlot;

	UFUNCTION()
	void OnRep_SelectedInventorySlot();

	virtual void OnInventorySelectionUpdated(const FInventorySlotKey& InSlotKey);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	float DefaultGravityScale;

	float DefaultAirControl;

	FActiveGameplayEffectHandle StaticEffect;
	FActiveGameplayEffectHandle InterruptEffect;
	const FGuid BaseStatsSource = FGuid(0x74FD8520, 0xB13E40AF, 0x9A9EFD04, 0xD49EE5D2);
	bool bBaseAttributeSourceReady = false;
	const FGuid StaticMovementSource = FGuid(0x0A2C1130, 0x4E6D4D2D, 0x8BC804B6, 0x0431316E);

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
	template <class T> T* GetAbilitySystemComponent() const
	{
		return Cast<T>(GetAbilitySystemComponent());
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template <class T> T* GetAttributeSet() const
	{
		return Cast<T>(GetAttributeSet());
	}

	virtual UAttributeSetBase* GetAttributeSet() const override;

	virtual UShapeComponent* GetCollisionComponent() const override;

	virtual UMeshComponent* GetMeshComponent() const override;

	virtual IInteractionAgentInterface* GetInteractingAgent() const override
	{
		return IInteractionAgentInterface::GetInteractingAgent();
	}

	template <class T> T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InClass) const
	{
		return GetDeterminesOutputObject(Cast<AActor>(GetInteractingAgent()), InClass);
	}

	virtual EInteractAgentType GetInteractAgentType() const override
	{
		return EInteractAgentType::Initiative;
	}

	virtual UInteractionComponent* GetInteractionComponent() const override;

	virtual UAbilityInventoryBase* GetInventory() const override;

	void BindInventory(UAbilityInventoryBase* InInventory);

	virtual FInventorySlotKey GetSelectedInventorySlot() const override { return SelectedInventorySlot; }

	void SetSelectedInventorySlot(const FInventorySlotKey& InSlotKey);

	FInventorySelectionChanged OnInventorySelectionChanged;
	FSimpleMulticastDelegate OnIdentityChanged;

	virtual UFSMComponent* GetFSMComponent() const override
	{
		return FSM;
	}

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsPlayer() const override;

	virtual bool IsEnemy(IAbilityPawnInterface* InTarget) const override;

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
	virtual FName GetNameA() const override
	{
		return Name;
	}

	UFUNCTION(BlueprintCallable)
	virtual void SetNameA(FName InName) override;

	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override
	{
		return RaceID;
	}

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override;

	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelA() const override
	{
		return Level;
	}

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
	virtual FTransform GetBirthTransform() const override
	{
		return BirthTransform;
	}

	UFUNCTION(BlueprintPure)
	virtual void GetMotionRate(float& OutMovementRate, float& OutRotationRate) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetMotionRate(float InMovementRate, float InRotationRate) override;

	UFUNCTION(BlueprintPure)
	virtual float GetDefaultGravityScale() const override
	{
		return DefaultGravityScale;
	}

	UFUNCTION(BlueprintPure)
	virtual float GetDefaultAirControl() const override
	{
		return DefaultAirControl;
	}

	UFUNCTION(BlueprintPure)
	virtual bool HasActionAbility(const FGameplayTag& InActionTag) const override;

	UFUNCTION(BlueprintPure)
	virtual FVitalityActionAbilityData GetActionAbility(const FGameplayTag& InActionTag) override;

	UFUNCTION(BlueprintPure)
	virtual TMap<FGameplayTag, FVitalityActionAbilityData> GetActionAbilities() const override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnRep_Controller() override;

	virtual void OnRep_PlayerState() override;
};
