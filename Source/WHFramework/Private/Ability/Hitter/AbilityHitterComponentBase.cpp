// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Hitter/AbilityHitterComponentBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"

UAbilityHitterComponentBase::UAbilityHitterComponentBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	SetRelativeScale3D(FVector(1, 1, 1));
	InitBoxExtent(FVector(20, 20, 20));
	SetGenerateOverlapEvents(false);
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Weapon"));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	return IsHitAble() && IsValid(InTarget) && InTarget->GetWorld() == GetWorld() && InTarget != GetOwnerActor() && !HitTargets.Contains(InTarget);
}

void UAbilityHitterComponentBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	if (!CanHitTarget(InTarget))
	{
		return;
	}
	UAbilityBase* Action = WindowAction.Get();
	UAbilitySystemComponentBase* AbilitySystem = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(GetOwnerActor());
	if (!Action || !AbilitySystem)
	{
		return;
	}
	HitTargets.Add(InTarget);
	FHitResult Hit = InHitResult;
	if (Hit.GetActor() != InTarget)
	{
		Hit = FHitResult(InTarget, Cast<UPrimitiveComponent>(InTarget->GetRootComponent()), InTarget->GetActorLocation(), -GetForwardVector());
	}
	FGameplayEventData Event;
	Event.EventTag = GameplayTags::Event_Hit_Attack;
	Event.Instigator = GetOwnerActor();
	Event.Target = InTarget;
	Event.OptionalObject = this;
	Event.OptionalObject2 = Action;
	Event.ContextHandle = AbilitySystem->MakeEffectContext();
	if (const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Action->GetActionRequest().AbilityHandle))
	{
		Event.ContextHandle.AddSourceObject(Spec->SourceObject.Get());
	}
	Event.ContextHandle.AddHitResult(Hit, true);
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwnerActor(), Event.EventTag, Event);
}

bool UAbilityHitterComponentBase::IsHitAble() const
{
	return GetOwnerActor() && GetOwnerActor()->HasAuthority() && GetGenerateOverlapEvents() && WindowAction.IsValid() && WindowAction->IsActive() && WindowAction->GetActionActivationId() == WindowActivationId;
}

void UAbilityHitterComponentBase::SetHitAble(bool bValue)
{
	const bool bEnable = bValue && GetOwnerActor() && GetOwnerActor()->HasAuthority() && WindowAction.IsValid() && WindowAction->IsActive() && WindowAction->GetActionActivationId() == WindowActivationId;
	SetGenerateOverlapEvents(bEnable);
	SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
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


bool UAbilityHitterComponentBase::BeginHitWindow(UAbilityBase* SourceAction)
{
	if (!GetOwnerActor() || !GetOwnerActor()->HasAuthority() || !SourceAction || !SourceAction->IsActive() || SourceAction->GetAvatarActorFromActorInfo() != GetOwnerActor() || !SourceAction->GetActionActivationId().IsValid())
	{
		return false;
	}
	UAbilitySystemComponentBase* AbilitySystem = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(GetOwnerActor());
	if (!AbilitySystem)
	{
		return false;
	}
	EndHitWindow(WindowActivationId);
	WindowAction = SourceAction;
	WindowActivationId = SourceAction->GetActionActivationId();
	WindowSystem = AbilitySystem;
	WindowFinishedDelegate = AbilitySystem->OnActionFinished.AddUObject(this, &ThisClass::OnActionFinished);
	ClearHitTargets();
	SetHitAble(true);
	return IsHitAble();
}

void UAbilityHitterComponentBase::EndHitWindow(const FGuid& ActivationId)
{
	if (WindowActivationId == ActivationId)
	{
		SetHitAble(false);
		if (WindowSystem.IsValid())
		{
			WindowSystem->OnActionFinished.Remove(WindowFinishedDelegate);
		}
		WindowSystem.Reset();
		WindowFinishedDelegate.Reset();
		WindowAction.Reset();
		WindowActivationId.Invalidate();
		ClearHitTargets();
	}
}

void UAbilityHitterComponentBase::OnActionFinished(const FAbilityActionResult& Result)
{
	EndHitWindow(Result.ActivationId);
}

void UAbilityHitterComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndHitWindow(WindowActivationId);
	Super::EndPlay(EndPlayReason);
}
