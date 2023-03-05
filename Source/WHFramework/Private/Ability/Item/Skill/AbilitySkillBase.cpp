// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/AbilitySkillBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "AbilitySystemBlueprintLibrary.h"

AAbilitySkillBase::AAbilitySkillBase()
{
	DurationTime = 0;
	SocketName = NAME_None;

	HitTargets = TArray<AActor*>();
}

void AAbilitySkillBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AAbilitySkillBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

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

bool AAbilitySkillBase::CanHitTarget_Implementation(AActor* InTarget)
{
	return InTarget != GetOwnerCharacter() && !HitTargets.Contains(InTarget);
}

void AAbilitySkillBase::OnHitTarget_Implementation(AActor* InTarget, const FHitResult& InHitResult)
{
	HitTargets.Add(InTarget);
	
	FGameplayEventData EventData;
	EventData.Instigator = GetOwnerCharacter();
	EventData.Target = InTarget;
	EventData.OptionalObject = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwnerCharacter(), FGameplayTag::RequestGameplayTag("Event.Hit.Skill"), EventData);
}

void AAbilitySkillBase::ClearHitTargets_Implementation()
{
	HitTargets.Empty();
}

void AAbilitySkillBase::SetHitAble_Implementation(bool bValue)
{

}
