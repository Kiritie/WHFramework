// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/Melee/AbilitySkillMeleeBase.h"

// Sets default values
AAbilitySkillMeleeBase::AAbilitySkillMeleeBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	DurationTime = 10;
}

// Called when the game starts or when spawned
void AAbilitySkillMeleeBase::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AAbilitySkillMeleeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
