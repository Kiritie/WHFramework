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
	AAbilityEquipBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;

public:
	virtual void Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem = FAbilityItem::Empty) override;

protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnHitTarget(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnAssemble();
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDischarge();

	UFUNCTION(BlueprintCallable)
	void SetCollisionEnable(bool InValue);

public:
	UFUNCTION(BlueprintPure)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	
	UFUNCTION(BlueprintPure)
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }
};
