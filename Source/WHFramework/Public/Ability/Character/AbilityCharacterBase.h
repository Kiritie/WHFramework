// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Character/Base/CharacterBase.h"
#include "Voxel/VoxelModuleTypes.h"

#include "AbilityCharacterBase.generated.h"

class UCharacterAttributeSetBase;
class UBoxComponent;
class AVoxelChunk;
class UVoxel;
class AController;
class UAbilityBase;
class UAbilitySystemComponentBase;
class AAbilitySkillBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDead);

/**
 * 角色
 */
UCLASS()
class DREAMWORLD_API AAbilityCharacterBase : public ACharacterBase, public IAbilityVitalityInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAbilityCharacterBase();
	
protected:
	// states
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStates")
	bool bDead;

protected:
	// stats
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FPrimaryAssetId ID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "CharacterStats")
	FString RaceID;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Components")
	UCharacterAttributeSetBase* AttributeSet;

public:
	UPROPERTY(BlueprintAssignable)
	FOnCharacterDead OnCharacterDead;

protected:
	float DefaultGravityScale;

	float DefaultAirControl;

	FVector MoveDirection;

	FAbilityData DefaultAbility;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData) override;

	virtual FSaveData* ToData(bool bSaved = true) override;

	UFUNCTION(BlueprintCallable)
	virtual void ResetData(bool bRefresh = false) override;

	UFUNCTION(BlueprintCallable)
	virtual void RefreshData() override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void Spawn() override;
		
	UFUNCTION(BlueprintCallable)
	virtual void Revive() override;
			
	UFUNCTION(BlueprintCallable)
	virtual void Death(AActor* InKiller = nullptr) override;

	virtual void Jump() override;

	UFUNCTION(BlueprintCallable)
	virtual void UnJump();

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
	virtual void AddWorldText(FString InContent, EWorldTextType InContentType, EWorldTextStyle InContentStyle) override;

	UFUNCTION(BlueprintCallable)
	float Distance(AAbilityCharacterBase* InTargetCharacter, bool bIgnoreRadius = true, bool bIgnoreZAxis = true);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetVisible(bool bVisible);
									
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetMotionRate(float InMovementRate, float InRotationRate);

public:
	UFUNCTION(BlueprintPure)
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure)
	UCharacterAttributeSetBase* GetAttributeSet() const { return AttributeSet; }

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsDead() const override { return bDead; }

public:
	UFUNCTION(BlueprintPure)
	virtual FString GetNameV() const override { return Name; }

	UFUNCTION(BlueprintCallable)
	virtual void SetNameV(const FString& InName) override;

	UFUNCTION(BlueprintPure)
	virtual FString GetRaceID() const override { return RaceID; }

	UFUNCTION(BlueprintCallable)
	virtual void SetRaceID(const FString& InRaceID) override;

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
	float GetMoveSpeed() const;
		
	UFUNCTION(BlueprintCallable)
	void SetMoveSpeed(float InValue);
			
	UFUNCTION(BlueprintPure)
	float GetRotationSpeed() const;
		
	UFUNCTION(BlueprintCallable)
	void SetRotationSpeed(float InValue);
			
	UFUNCTION(BlueprintPure)
	float GetJumpForce() const;
		
	UFUNCTION(BlueprintCallable)
	void SetJumpForce(float InValue);

	UFUNCTION(BlueprintPure)
	virtual float GetPhysicsDamage() const override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetMagicDamage() const override;

	UFUNCTION(BlueprintPure)
	FVector GetMoveVelocity(bool bIgnoreZ = true) const;

	UFUNCTION(BlueprintPure)
	FVector GetMoveDirection(bool bIgnoreZ = true) const;
		
	UFUNCTION(BlueprintPure)
	float GetRadius() const;

	UFUNCTION(BlueprintPure)
	float GetHalfHeight() const;
	
	UFUNCTION(BlueprintPure)
	AController* GetOwnerController() const { return OwnerController; }

	UFUNCTION(BlueprintCallable)
	void SetOwnerController(AController* InOwnerController) { this->OwnerController = InOwnerController; }

public:
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;
	
	virtual void HandleNameChanged(const FString& NewValue) override;

	virtual void HandleRaceIDChanged(const FString& NewValue) override;

	virtual void HandleLevelChanged(int32 NewValue, int32 DeltaValue = 0) override;

	virtual void HandleEXPChanged(int32 NewValue, int32 DeltaValue = 0) override;

	virtual void HandleHealthChanged(float NewValue, float DeltaValue = 0.f) override;
	
	virtual void HandleMaxHealthChanged(float NewValue, float DeltaValue = 0.f) override;

	virtual void HandleMoveSpeedChanged(float NewValue, float DeltaValue = 0.f);

	virtual void HandleRotationSpeedChanged(float NewValue, float DeltaValue = 0.f);
	
	virtual void HandleJumpForceChanged(float NewValue, float DeltaValue = 0.f);
};
