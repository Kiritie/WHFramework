// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/AssetModuleTypes.h"
#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "AbilityItemBase.generated.h"

class AAbilityCharacterBase;

/**
 * 物品基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityItemBase : public AWHActor, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAbilityItemBase();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, Category = "Default")
	AAbilityCharacterBase* OwnerCharacter;

protected:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;

public:
	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter);
		
	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem);

public:
	template<class T >
	T& GetItemData() const
	{
		return Item.GetData<T>();
	}

	UAbilityItemDataBase& GetItemData() const
	{
		return Item.GetData();
	}

	template<class T>
	T* GetOwnerCharacter() const
	{
		return Cast<T>(OwnerCharacter);
	}

	UFUNCTION(BlueprintPure)
	AAbilityCharacterBase* GetOwnerCharacter() const { return OwnerCharacter; }
};
