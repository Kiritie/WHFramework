// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilityEquipBase.generated.h"

class AAbilityCharacterBase;
class UStaticMeshComponent;

/**
 * 装备基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityEquipBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAbilityEquipBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
		
	UFUNCTION(BlueprintCallable)
	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter) override;

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetVisible(bool bVisible);

	UFUNCTION(BlueprintNativeEvent)
	void OnAssemble();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDischarge();

public:
	UFUNCTION(BlueprintPure)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
};
