// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/AbilitySkillBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"

// Sets default values
AAbilitySkillBase::AAbilitySkillBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	DurationTime = 0;
	SocketName = NAME_None;
}

// Called when the game starts or when spawned
void AAbilitySkillBase::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, [this]() { Destroy(); }, DurationTime, false);
}

// Called every frame
void AAbilitySkillBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AAbilitySkillBase::Initialize(AAbilityCharacterBase* InOwnerCharacter, const FPrimaryAssetId& InSkillID)
{
	if (InOwnerCharacter)
	{
		OwnerCharacter = InOwnerCharacter;
		SkillAbilityID = InSkillID;
		SetActorLocation(OwnerCharacter->GetMesh()->GetSocketLocation(SocketName));
		SetActorRotation(OwnerCharacter->GetActorRotation());
	}
}

void AAbilitySkillBase::Destroyed()
{
	Super::Destroyed();
	GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
}
