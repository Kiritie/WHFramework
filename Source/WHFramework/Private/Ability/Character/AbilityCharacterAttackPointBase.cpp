// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Character/AbilityCharacterAttackPointBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterAttackPointBase::UAbilityCharacterAttackPointBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	SetRelativeScale3D(FVector(1, 1, 1));
	InitBoxExtent(FVector(20, 20, 20));
	SetGenerateOverlapEvents(false);
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Weapon"));
	OnComponentBeginOverlap.AddDynamic(this, &UAbilityCharacterAttackPointBase::OnBeginOverlap);

	HitTargets = TArray<AActor*>();
}

void UAbilityCharacterAttackPointBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(CanHitTarget(OtherActor))
	{
		OnHitTarget(OtherActor, SweepResult);
	}
}

bool UAbilityCharacterAttackPointBase::CanHitTarget(AActor* InTarget) const
{
	return InTarget != GetOwnerCharacter() && !HitTargets.Contains(InTarget);
}

void UAbilityCharacterAttackPointBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	HitTargets.Add(InTarget);
	
	FGameplayEventData EventData;
	EventData.Instigator = GetOwnerCharacter();
	EventData.Target = InTarget;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwnerCharacter(), GameplayTags::EventTag_Hit_Attack, EventData);
}

void UAbilityCharacterAttackPointBase::ClearHitTargets()
{
	HitTargets.Empty();
}

void UAbilityCharacterAttackPointBase::SetHitAble(bool bValue)
{
	SetGenerateOverlapEvents(bValue);
}

AAbilityCharacterBase* UAbilityCharacterAttackPointBase::GetOwnerCharacter() const
{
	return Cast<AAbilityCharacterBase>(GetOwner());
}
