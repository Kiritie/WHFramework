// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "AbilityActorInterface.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "Common/Base/WHActor.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "SaveGame/Base/SaveDataInterface.h"

#include "AbilityActorBase.generated.h"

class UInteractionComponent;
class UActorAttributeSetBase;
class UActorAbilityBase;
class UAbilitySystemComponentBase;
class UBoxComponent;
class UAttributeSetBase;
class UAbilityInventoryBase;
class UAbilityActorDataBase;

/**
 * Ability Actor基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityActorBase : public AWHActor, public IAbilityActorInterface, public IPrimaryEntityInterface, public IInteractionAgentInterface, public IAbilityInventoryAgentInterface, public ISaveDataInterface
{
	GENERATED_BODY()

public:
	AAbilityActorBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	virtual void ResetData();

public:
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

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UAbilitySystemComponentBase* AbilitySystem;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAttributeSetBase* AttributeSet;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* Interaction;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAbilityInventoryBase* Inventory;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActorStats")
	FPrimaryAssetId AssetID;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "ActorStats")
	int32 Level;

public:
	virtual void OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData) override;

public:
	template<class T>
	T& GetActorData() const
	{
		return static_cast<T&>(GetActorData());
	}
	
	UAbilityActorDataBase& GetActorData() const;

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

	virtual IInteractionAgentInterface* GetInteractingAgent() const override { return IInteractionAgentInterface::GetInteractingAgent(); }

	template<class T>
	T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InClass) const { return GetDeterminesOutputObject(Cast<AActor>(GetInteractingAgent()), InClass); }

	virtual UInteractionComponent* GetInteractionComponent() const override { return Interaction; }
	
	virtual UAbilityInventoryBase* GetInventory() const override { return Inventory; }

public:
	virtual FPrimaryAssetId GetAssetID_Implementation() const override { return AssetID; }
	
	virtual void SetAssetID_Implementation(const FPrimaryAssetId& InID) override { AssetID = InID; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetLevelV() const override { return Level; }

	UFUNCTION(BlueprintCallable)
	virtual bool SetLevelV(int32 InLevel) override;
	
	UFUNCTION(BlueprintPure)
	virtual float GetRadius() const override;

	UFUNCTION(BlueprintPure)
	virtual float GetHalfHeight() const override;
};
