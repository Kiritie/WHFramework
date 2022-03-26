// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilitySkillBase.generated.h"

/**
 * 技能
 */
UCLASS()
class DREAMWORLD_API AAbilitySkillBase : public AAbilityItemBase
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAbilitySkillBase();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float DurationTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Default")
	FName SkillAbilityIndex;

private:
	FTimerHandle DestroyTimer;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter, const FName& InSkillIndex);

	virtual void Destroyed() override;

public:
	UFUNCTION(BlueprintPure)
	FName GetSkillIndex() const { return SkillAbilityIndex; }
};
