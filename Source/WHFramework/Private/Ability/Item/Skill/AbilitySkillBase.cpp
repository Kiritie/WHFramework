// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/AbilitySkillBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Item/AbilityItemDataBase.h"

AAbilitySkillBase::AAbilitySkillBase()
{
	DurationTime = 0;
	SocketName = NAME_None;
}

void AAbilitySkillBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AAbilitySkillBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
}

void AAbilitySkillBase::Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerCharacter, InItem);

	SetActorLocation(OwnerCharacter->GetMesh()->GetSocketLocation(SocketName));
	SetActorRotation(OwnerCharacter->GetActorRotation());

	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, [this](){
		UObjectPoolModuleBPLibrary::DespawnObject(this);	
	}, DurationTime, false);
}
