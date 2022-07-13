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

public:
	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter) override;

	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem) override;

	virtual void Destroyed() override;

public:
	UFUNCTION(BlueprintPure)
	FPrimaryAssetId GetSkillID() const;
};
