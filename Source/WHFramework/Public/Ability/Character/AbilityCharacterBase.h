// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Ability/Interaction/InteractionAgentInterface.h"
#include "Ability/PickUp/AbilityPickerInterface.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Character/Base/CharacterBase.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Voxel/VoxelModuleTypes.h"

#include "AbilityCharacterBase.generated.h"

class UFSMComponent;
class UCharacterInteractionComponent;
class UCharacterAttributeSetBase;
class UBoxComponent;
class AVoxelChunk;
class UVoxel;
class AController;
class UAbilityBase;
class UAbilitySystemComponentBase;
class AAbilitySkillBase;

/**
 * Ability Character基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityCharacterBase : public ACharacterBase, public IAbilityVitalityInterface, public IAbilitySystemInterface, public IFSMAgentInterface, public IAbilityPickerInterface, public IInteractionAgentInterface, public ISaveDataInterface
{
	GENERATED_BODY()

	friend class UAbilityCharacterState_Death;
	friend class UAbilityCharacterState_Default;
	friend class UAbilityCharacterState_Fall;
	friend class UAbilityCharacterState_Idle;
	friend class UAbilityCharacterState_Jump;
	friend class UAbilityCharacterState_Static;
	friend class UAbilityCharacterState_Walk;

public:
	AAbilityCharacterBase();
	
protected:
	// stats
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FName RaceID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 EXP;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 BaseEXP;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	int32 EXPFactor;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	float MovementRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	float RotationRate;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	AController* OwnerController;

protected:
	FGameplayTag DeadTag;
	
	FGameplayTag DyingTag;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterAttributeSetBase* AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCharacterInteractionComponent* Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UFSMComponent* FSM;

protected:
	bool bASCInputBound;

	float DefaultGravityScale;

	float DefaultAirControl;

	FVector MoveDirection;

	FAbilityData DefaultAbility;

protected:
	virtual void BeginPlay() override;

	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BindASCInput();

	virtual void OnFiniteStateChanged(UFiniteStateBase* InFiniteState) override;

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	virtual void RefreshFiniteState();

public:
	virtual void Tick(float DeltaTime) override;

	virtual void Serialize(FArchive& Ar) override;
			
	virtual void Death(IAbilityVitalityInterface* InKiller = nullptr) override;

	virtual void Revive(IAbilityVitalityInterface* InRescuer = nullptr) override;

	virtual void Jump() override;

	UFUNCTION(BlueprintCallable)
	virtual void UnJump();

	virtual void PickUp(AAbilityPickUpBase* InPickUp) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

public:
	UFUNCTION(BlueprintCallable)
	virtual FGameplayAbilitySpecHandle AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel = 1) override;

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bAllowRemoteActivation = false) override;

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation = false) override;

	UFUNCTION(BlueprintCallable)
	virtual bool ActiveAbilityByTag(const FGameplayTagContainer& AbilityTags, bool bAllowRemoteActivation = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void CancelAbility(UAbilityBase* Ability) override;

	UFUNCTION(BlueprintCallable)
	virtual void CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle) override;

	UFUNCTION(BlueprintCallable)
	virtual void CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore = nullptr) override;
	
	UFUNCTION(BlueprintCallable)
	virtual void CancelAllAbilities(UAbilityBase* Ignore = nullptr) override;
	
	UFUNCTION(BlueprintCallable)
	virtual FActiveGameplayEffectHandle ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass) override;
	
	UFUNCTION(BlueprintCallable)
	virtual FActiveGameplayEffectHandle ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle) override;
		
	UFUNCTION(BlueprintCallable)
	virtual FActiveGameplayEffectHandle ApplyEffectBySpec(const FGameplayEffectSpec& Spec) override;

	UFUNCTION(BlueprintCallable)
	virtual bool RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1) override;

	UFUNCTION(BlueprintPure)
	virtual void GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities) override;

	UFUNCTION(BlueprintPure)
	virtual bool GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo) override;
				
	UFUNCTION(BlueprintCallable)
	virtual void ModifyEXP(float InDeltaValue) override;
				
	UFUNCTION(BlueprintCallable)
	virtual void ModifyHealth(float InDeltaValue) override;

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f, bool bForce = false) override;

public:
	UFUNCTION(BlueprintCallable)
	float Distance(AAbilityCharacterBase* InTargetCharacter, bool bIgnoreRadius = true, bool bIgnoreZAxis = true);
									
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetMotionRate(float InMovementRate, float InRotationRate);

public:
	UFUNCTION(BlueprintPure)
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template<class T>
	T& GetCharacterData() const
	{
		return static_cast<T&>(GetCharacterData());
	}
	
	UAbilityCharacterDataBase& GetCharacterData() const;

	template<class T>
	T* GetAttributeSet() const
	{
		return Cast<T>(AttributeSet);
	}
	UFUNCTION(BlueprintPure)
	UCharacterAttributeSetBase* GetAttributeSet() const { return AttributeSet; }
	
	UFUNCTION(BlueprintPure)
	virtual UInteractionComponent* GetInteractionComponent() const override;

	UFUNCTION(BlueprintPure)
	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = false) const override;

	UFUNCTION(BlueprintPure)
	virtual bool IsDying() const override;

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
	virtual void SetLevelV(int32 InLevel) override;

	UFUNCTION(BlueprintPure)
	virtual int32 GetEXP() const override { return EXP; }
		
	UFUNCTION(BlueprintCallable)
	virtual void SetEXP(int32 InEXP) override;
	
	UFUNCTION(BlueprintPure)
	virtual int32 GetBaseEXP() const override { return BaseEXP; }
	
	UFUNCTION(BlueprintPure)
	virtual int32 GetEXPFactor() const override { return EXPFactor; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetMaxEXP() const override;

	UFUNCTION(BlueprintPure)
	virtual int32 GetTotalEXP() const override;

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;
		
	UFUNCTION(BlueprintPure)
	virtual float GetHealth() const override;
		
	UFUNCTION(BlueprintCallable)
	virtual void SetHealth(float InValue) override;

	UFUNCTION(BlueprintPure)
	virtual float GetMaxHealth() const override;
		
	UFUNCTION(BlueprintCallable)
	virtual void SetMaxHealth(float InValue) override;
			
	UFUNCTION(BlueprintPure)
	virtual float GetMoveSpeed() const;
		
	UFUNCTION(BlueprintCallable)
	virtual void SetMoveSpeed(float InValue);
			
	UFUNCTION(BlueprintPure)
	virtual float GetRotationSpeed() const;
		
	UFUNCTION(BlueprintCallable)
	virtual void SetRotationSpeed(float InValue);
			
	UFUNCTION(BlueprintPure)
	virtual float GetJumpForce() const;
		
	UFUNCTION(BlueprintCallable)
	virtual void SetJumpForce(float InValue);

	UFUNCTION(BlueprintPure)
	virtual float GetPhysicsDamage() const override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetMagicDamage() const override;

	UFUNCTION(BlueprintPure)
	virtual FVector GetMoveVelocity(bool bIgnoreZ = true) const;

	UFUNCTION(BlueprintPure)
	virtual FVector GetMoveDirection(bool bIgnoreZ = true) const;
		
	UFUNCTION(BlueprintPure)
	virtual float GetRadius() const;

	UFUNCTION(BlueprintPure)
	virtual float GetHalfHeight() const;

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;
	
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;

public:
	virtual void OnRep_Controller() override;

	virtual void OnRep_PlayerState() override;
};
