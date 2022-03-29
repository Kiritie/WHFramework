// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/AbilitySkillMelee.h"

// Sets default values
AAbilitySkillMelee::AAbilitySkillMelee()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	DurationTime = 10;
}

// Called when the game starts or when spawned
void AAbilitySkillMelee::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AAbilitySkillMelee::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
