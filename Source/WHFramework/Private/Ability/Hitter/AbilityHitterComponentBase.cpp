// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Hitter/AbilityHitterComponentBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Character/AbilityCharacterBase.h"

UAbilityHitterComponentBase::UAbilityHitterComponentBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
	SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	SetRelativeScale3D(FVector(1, 1, 1));
	InitBoxExtent(FVector(20, 20, 20));
	SetGenerateOverlapEvents(false);
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Weapon"));
	OnComponentBeginOverlap.AddDynamic(this, &UAbilityHitterComponentBase::OnBeginOverlap);

	HitTargets = TArray<AActor*>();
}

void UAbilityHitterComponentBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(CanHitTarget(OtherActor))
	{
		OnHitTarget(OtherActor, SweepResult);
	}
}

bool UAbilityHitterComponentBase::CanHitTarget(AActor* InTarget) const
{
	return InTarget != GetOwnerActor() && !HitTargets.Contains(InTarget);
}

void UAbilityHitterComponentBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	HitTargets.Add(InTarget);
	
	FGameplayEventData EventData;
	EventData.Instigator = GetOwnerActor();
	EventData.Target = InTarget;
	EventData.OptionalObject = this;
	EventData.ContextHandle.AddHitResult(InHitResult);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwnerActor(), GameplayTags::Event_Hit_Attack, EventData);
}

bool UAbilityHitterComponentBase::IsHitAble() const
{
	return GetGenerateOverlapEvents();
}

void UAbilityHitterComponentBase::SetHitAble(bool bValue)
{
	SetGenerateOverlapEvents(bValue);
}

void UAbilityHitterComponentBase::ClearHitTargets()
{
	HitTargets.Empty();
}

TArray<AActor*> UAbilityHitterComponentBase::GetHitTargets() const
{
	return HitTargets;
}

AActor* UAbilityHitterComponentBase::GetOwnerActor() const
{
	return GetOwner();
}
