// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySkillBase.h"
#include "AbilitySkillMelee.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * ��ս����
 */
UCLASS()
class DREAMWORLD_API AAbilitySkillMelee : public AAbilitySkillBase
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAbilitySkillMelee();


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
