// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/AbilityItemBase.h"
#include "AbilitySkillBase.generated.h"

/**
 * 技能
 */
UCLASS()
class WHFRAMEWORK_API AAbilitySkillBase : public AAbilityItemBase
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
	FPrimaryAssetId SkillAbilityID;

private:
	FTimerHandle DestroyTimer;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter) override;

	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter, const FPrimaryAssetId& InSkillID);

	virtual void Destroyed() override;

public:
	UFUNCTION(BlueprintPure)
	FPrimaryAssetId GetSkillID() const { return SkillAbilityID; }
};
