// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/CharacterAttackPointComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Character/AbilityCharacterBase.h"

UCharacterAttackPointComponent::UCharacterAttackPointComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	SetRelativeScale3D(FVector(1, 1, 1));
	InitBoxExtent(FVector(20, 20, 20));
	SetGenerateOverlapEvents(false);
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Weapon"));
	OnComponentBeginOverlap.AddDynamic(this, &UCharacterAttackPointComponent::OnBeginOverlap);

	HitTargets = TArray<AActor*>();
}

void UCharacterAttackPointComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(Execute_CanHitTarget(this, OtherActor))
	{
		Execute_OnHitTarget(this, OtherActor, SweepResult);
	}
}

bool UCharacterAttackPointComponent::CanHitTarget_Implementation(AActor* InTarget)
{
	return InTarget != GetOwnerCharacter() && !HitTargets.Contains(InTarget);
}

void UCharacterAttackPointComponent::OnHitTarget_Implementation(AActor* InTarget, const FHitResult& InHitResult)
{
	HitTargets.Add(InTarget);
	
	FGameplayEventData EventData;
	EventData.Instigator = GetOwnerCharacter();
	EventData.Target = InTarget;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwnerCharacter(), FGameplayTag::RequestGameplayTag("Event.Hit.Attack"), EventData);
}

void UCharacterAttackPointComponent::ClearHitTargets_Implementation()
{
	HitTargets.Empty();
}

void UCharacterAttackPointComponent::SetHitAble_Implementation(bool bValue)
{
	SetGenerateOverlapEvents(bValue);
}

AAbilityCharacterBase* UCharacterAttackPointComponent::GetOwnerCharacter() const
{
	return Cast<AAbilityCharacterBase>(GetOwner());
}
