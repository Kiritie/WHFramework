// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilityVitalityInterface.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Interaction/InteractionAgentInterface.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Agent/VoxelAgentInterface.h"

#include "AbilityVitalityBase.generated.h"

class UFSMComponent;
class UVitalityInteractionComponent;
class UVitalityAttributeSetBase;
class UVitalityAbilityBase;
class UAbilitySystemComponentBase;
class UBoxComponent;
class UAttributeSetBase;

/**
 * Ability Vitality基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityVitalityBase : public AAbilityActorBase, public IAbilityVitalityInterface, public IFSMAgentInterface, public IVoxelAgentInterface, public IPrimaryEntityInterface, public IInteractionAgentInterface, public ISaveDataInterface
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
	UVitalityInteractionComponent* Interaction;

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
	int32 EXP;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	int32 BaseEXP;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	int32 EXPFactor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FVoxelItem GeneratingVoxelItem;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FVoxelItem SelectedVoxelItem;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual int32 GetLimit_Implementation() const override { return 1000; }
	
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

	virtual void OnFiniteStateChanged(UFiniteStateBase* InFiniteState) override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void Death(IAbilityVitalityInterface* InKiller = nullptr) override;

	virtual void Revive(IAbilityVitalityInterface* InRescuer = nullptr) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;
	
	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

public:
	virtual FGameplayAbilitySpecHandle AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel = 1) override;

	virtual bool ActiveAbility(FGameplayAbilitySpecHandle AbilityHandle, bool bAllowRemoteActivation = false) override;

	virtual bool ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation = false) override;

	virtual bool ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation = false) override;

	virtual void CancelAbility(UAbilityBase* Ability) override;

	virtual void CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle) override;

	virtual void CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore=nullptr) override;
	
	virtual void CancelAllAbilities(UAbilityBase* Ignore=nullptr) override;
	
	virtual FActiveGameplayEffectHandle ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass) override;
	
	virtual FActiveGameplayEffectHandle ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle) override;
		
	virtual FActiveGameplayEffectHandle ApplyEffectBySpec(const FGameplayEffectSpec& Spec) override;

	virtual bool RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1) override;

	virtual void GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities) override;
	
	virtual bool GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo) override;

	UFUNCTION(BlueprintCallable)
	virtual void ModifyHealth(float InDeltaValue) override;
				
	UFUNCTION(BlueprintCallable)
	virtual void ModifyEXP(float InDeltaValue) override;

	UFUNCTION(BlueprintCallable)
	virtual bool GenerateVoxel(FVoxelItem& InVoxelItem) override;

	virtual bool GenerateVoxel(FVoxelItem& InVoxelItem, const FVoxelHitResult& InVoxelHitResult) override;

	UFUNCTION(BlueprintCallable)
	virtual bool DestroyVoxel(FVoxelItem& InVoxelItem) override;

	virtual bool DestroyVoxel(const FVoxelHitResult& InVoxelHitResult) override;

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsDead(bool bCheckDying = false) const override;

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

	virtual int32 GetLevelV() const override { return Super::GetLevelV(); }

	virtual void SetLevelV(int32 InLevel) override { Super::SetLevelV(InLevel); }

	UFUNCTION(BlueprintPure)
	virtual int32 GetEXP() const override { return EXP; }
		
	UFUNCTION(BlueprintCallable)
	virtual void SetEXP(int32 InEXP) override { EXP = InEXP; }
		
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
	virtual float GetPhysicsDamage() const override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetMagicDamage() const override;

	virtual FVector GetWorldLocation() const override { return GetActorLocation(); }

	virtual FVoxelItem& GetGeneratingVoxelItem() override { return GeneratingVoxelItem; }

	virtual void SetGeneratingVoxelItem(FVoxelItem InGeneratingVoxelItem) override { GeneratingVoxelItem = InGeneratingVoxelItem; }

	virtual FVoxelItem& GetSelectedVoxelItem() override { return SelectedVoxelItem; }

	virtual void SetSelectedVoxelItem(FVoxelItem InSelectedVoxelItem) override { SelectedVoxelItem = InSelectedVoxelItem; }

	template<class T>
	T& GetVitalityData() const
	{
		return static_cast<T&>(GetVitalityData());
	}
	
	UAbilityVitalityDataBase& GetVitalityData() const;

	UFUNCTION(BlueprintPure)
	virtual UInteractionComponent* GetInteractionComponent() const override;

	UFUNCTION(BlueprintPure)
	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;
	
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;
};
