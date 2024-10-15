// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "Common/Base/WHActor.h"
#include "AbilityItemBase.generated.h"

/**
 * 物品基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityItemBase : public AWHActor, public IPrimaryEntityInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAbilityItemBase();

protected:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION(BlueprintNativeEvent)
	void Initialize(AActor* InOwnerActor, const FAbilityItem& InItem = FAbilityItem::Empty);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AActor* OwnerActor;

public:
	virtual FPrimaryAssetId GetAssetID_Implementation() const override { return Item.ID; }

	virtual void SetAssetID_Implementation(const FPrimaryAssetId& InID) override { Item.ID = InID; }
	
	template<class T >
	T& GetItemData() const
	{
		return Item.GetData<T>();
	}

	UAbilityItemDataBase& GetItemData() const
	{
		return Item.GetData();
	}

	FAbilityItem& GetItem() { return Item; }

	template<class T>
	T* GetOwnerActor() const
	{
		return Cast<T>(OwnerActor);
	}

	UFUNCTION(BlueprintPure)
	AActor* GetOwnerActor() const { return OwnerActor; }
};
