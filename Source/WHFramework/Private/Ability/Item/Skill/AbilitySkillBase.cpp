// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/AbilitySkillBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

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

void AAbilitySkillBase::Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerActor, InItem);

	SetActorLocation(GetOwnerActor<AAbilityCharacterBase>()->GetMesh()->GetSocketLocation(SocketName));
	SetActorRotation(GetOwnerActor<AAbilityCharacterBase>()->GetActorRotation());

	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, [this](){
		UObjectPoolModuleStatics::DespawnObject(this);
	}, DurationTime, false);
}

bool AAbilitySkillBase::CanHitTarget(AActor* InTarget) const
{
	return InTarget != OwnerActor && !HitTargets.Contains(InTarget);
}

void AAbilitySkillBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	HitTargets.Add(InTarget);
	
	FGameplayEventData EventData;
	EventData.Instigator = OwnerActor;
	EventData.Target = InTarget;
	EventData.OptionalObject = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, FGameplayTag::RequestGameplayTag("Event.Hit.Skill"), EventData);
}

void AAbilitySkillBase::ClearHitTargets()
{
	HitTargets.Empty();
}

void AAbilitySkillBase::SetHitAble(bool bValue)
{

}
