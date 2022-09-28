// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilitySkillBase.generated.h"

/**
 * 技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilitySkillBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	AAbilitySkillBase();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default")
	float DurationTime;

private:
	FTimerHandle DestroyTimer;

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;
	
public:
	virtual void Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem = FAbilityItem::Empty) override;
};
