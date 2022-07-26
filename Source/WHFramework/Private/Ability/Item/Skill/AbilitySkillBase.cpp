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

void AAbilitySkillBase::Initialize(AAbilityCharacterBase* InOwnerCharacter)
{
	Super::Initialize(InOwnerCharacter);

	SetActorLocation(OwnerCharacter->GetMesh()->GetSocketLocation(SocketName));
	SetActorRotation(OwnerCharacter->GetActorRotation());
}

void AAbilitySkillBase::Initialize(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize(InOwnerCharacter, InItem);
}

void AAbilitySkillBase::Destroyed()
{
	Super::Destroyed();
	GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
}

FPrimaryAssetId AAbilitySkillBase::GetSkillID() const
{
	return GetItemData().GetPrimaryAssetId();
}
