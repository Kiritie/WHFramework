// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Ability/Attributes/CharacterAttributeSetBase.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "Ability/PickUp/AbilityPickerInterface.h"
#include "Character/Base/CharacterBase.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Pawn/AbilityPawnInterface.h"
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
class AAbilitySkillBase;
class UAbilityCharacterInventoryBase;

/**
 * Ability Character基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityCharacterBase : public ACharacterBase, public IAbilityPawnInterface, public IFSMAgentInterface, public IAbilityPickerInterface, public IInteractionAgentInterface, public ISaveDataInterface, public IAbilityInventoryAgentInterface, public ITargetingAgentInterface
{
	GENERATED_BODY()

	friend class UAbilityCharacterState_Death;
	friend class UAbilityCharacterState_Default;
	friend class UAbilityCharacterState_Fall;
	friend class UAbilityCharacterState_Jump;
	friend class UAbilityCharacterState_Static;
	friend class UAbilityCharacterState_Walk;

public:
	AAbilityCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//////////////////////////////////////////////////////////////////////////
	/// WHActor
public:
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	// stats
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FName RaceID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 Level;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	float MovementRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	float RotationRate;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	AController* OwnerController;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterAttributeSetBase* AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* Interaction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilityCharacterInventoryBase* Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	bool bASCInputBound;

	float DefaultGravityScale;

	float DefaultAirControl;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BindASCInput();

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void ResetData();

	virtual void RefreshState();

	virtual void OnFiniteStateChanged(UFiniteStateBase* InFiniteState) override;

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

public:
	virtual void Death(IAbilityVitalityInterface* InKiller = nullptr) override;

	virtual void Kill(IAbilityVitalityInterface* InTarget) override;

	virtual void Revive(IAbilityVitalityInterface* InRescuer = nullptr) override;

	virtual void Jump() override;

	UFUNCTION(BlueprintCallable)
	virtual void UnJump();

public:
	virtual bool OnPickUp(AAbilityPickUpBase* InPickUp) override;

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
	UFUNCTION(BlueprintCallable)
	float GetDistance(AAbilityCharacterBase* InTargetCharacter, bool bIgnoreRadius = true, bool bIgnoreZAxis = true);
									
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetMotionRate(float InMovementRate, float InRotationRate);

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

	virtual IInteractionAgentInterface* GetInteractingAgent() const override { return IInteractionAgentInterface::GetInteractingAgent(); }

	template<class T>
	T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InClass) const { return GetDeterminesOutputType(Cast<AActor>(GetInteractingAgent()), InClass); }

	virtual UInteractionComponent* GetInteractionComponent() const override;
	
	virtual UAbilityInventoryBase* GetInventory() const override;

	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsPlayer() const override;

	virtual bool IsEnemy(IAbilityPawnInterface* InTarget) const override;

	virtual bool IsTargetable_Implementation() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsActive(bool bNeedNotDead = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = true) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsDying() const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsFalling(bool bMovementMode = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool IsWalking(bool bMovementMode = false) const;

	UFUNCTION(BlueprintPure)
	virtual bool IsJumping() const;

public:
	UFUNCTION(BlueprintPure)
	virtual FName GetNameV() const override { return Name; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNameV(FName InName) override;

	UFUNCTION(BlueprintPure)
	virtual FName GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(FName InRaceID) override;

	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelV() const override { return Level; }
	
	UFUNCTION(BlueprintCallable)
	virtual bool SetLevelV(int32 InLevel) override;

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;
		
	UFUNCTION(BlueprintPure)
	virtual float GetDefaultGravityScale() const { return DefaultGravityScale; }

	UFUNCTION(BlueprintPure)
	virtual float GetDefaultAirControl() const { return DefaultAirControl; }

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

	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, MoveSpeed)

	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, RotationSpeed)

	ATTRIBUTE_ACCESSORS(UCharacterAttributeSetBase, JumpForce)

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;
	
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

public:
	virtual void OnRep_Controller() override;

	virtual void OnRep_PlayerState() override;
};
