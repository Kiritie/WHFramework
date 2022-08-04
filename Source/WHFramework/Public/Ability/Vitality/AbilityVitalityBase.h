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
class WHFRAMEWORK_API AAbilityVitalityBase : public AWHActor, public IAbilityVitalityInterface, public IFSMAgentInterface, public IVoxelAgentInterface, public IPrimaryEntityInterface, public IInteractionAgentInterface, public ISaveDataInterface
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UVitalityAttributeSetBase* AttributeSet;

protected:
	// stats
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VitalityStats")
	FPrimaryAssetId AssetID;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName Name;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FName RaceID;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "ActorStats")
	int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VitalityStats")
	FVoxelItem GenerateVoxelItem;

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

	virtual UFSMComponent* GetFSMComponent() const override { return FSM; }

public:
	virtual FVector GetWorldLocation() const override { return GetActorLocation(); }

	virtual FVoxelItem& GetGenerateVoxelItem() override { return GenerateVoxelItem; }

	virtual void SetGenerateVoxelItem(const FVoxelItem& InGenerateVoxelItem) override { GenerateVoxelItem = InGenerateVoxelItem; }

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
	virtual void SetLevelV(int32 InLevel) override { Level = InLevel; }

	UFUNCTION(BlueprintPure)
	virtual FString GetHeadInfo() const override;

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Health)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxHealth)
			
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, Exp)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MaxExp)

	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, PhysicsDamage)
	
	ATTRIBUTE_ACCESSORS(UVitalityAttributeSetBase, MagicDamage)

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;
	
	virtual void HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor) override;
};
