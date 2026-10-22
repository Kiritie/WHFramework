#include "Ability/Components/AbilityControlComponent.h"

#include "Ability/AbilityActionTypes.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Effects/AbilityControlEffect.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "AIController.h"
#include "Character/Components/CharacterMovementComponentBase.h"
#include "Common/CommonModuleTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

UAbilityControlComponent::UAbilityControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	for (FGameplayTag Tag : {AbilityActionTags::StaticControl.GetTag(), GameplayTags::State_Vitality_Interrupting.GetTag()})
	{
		FAbilityControlRule Rule;
		Rule.Tag = Tag;
		Rule.bBlockMovement = true;
		Rule.bBlockRotation = true;
		Rule.bBlockAllActions = true;
		Rules.Add(Rule);
	}
}

void UAbilityControlComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IAbilityActorInterface* Actor = Cast<IAbilityActorInterface>(GetOwner()))
	{
		BindAbilitySystem(Cast<UAbilitySystemComponentBase>(Actor->GetAbilitySystemComponent()));
	}
}

void UAbilityControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindAbilitySystem(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UAbilityControlComponent::BindAbilitySystem(UAbilitySystemComponentBase* InSystem)
{
	if (InSystem && (InSystem->GetAvatarActor() != GetOwner() || InSystem->GetWorld() != GetWorld()))
	{
		return;
	}
	if (System.Get() == InSystem)
	{
		RefreshProjection();
		return;
	}
	UnregisterTags();
	ResetControls();
	ReleaseMovement();
	System = InSystem;
	StaticHandle.Invalidate();
	RegisterTags();
	RefreshProjection();
}

void UAbilityControlComponent::SetAdditionalRules(const TArray<FAbilityControlRule>& InRules)
{
	UnregisterTags();
	Rules.SetNum(2);
	for (const FAbilityControlRule& Rule : InRules)
	{
		if (Rule.Tag.IsValid())
		{
			Rules.Add(Rule);
		}
	}
	RegisterTags();
	RefreshProjection();
}

void UAbilityControlComponent::RegisterTags()
{
	UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem)
	{
		return;
	}
	FGameplayTagContainer Tags;
	Tags.AddTag(GameplayTags::State_Vitality_Active);
	Tags.AddTag(GameplayTags::State_Vitality_Dead);
	Tags.AddTag(GameplayTags::State_Vitality_Dying);
	for (const FAbilityControlRule& Rule : Rules)
	{
		Tags.AddTag(Rule.Tag);
	}
	for (const FGameplayTag& Tag : Tags)
	{
		TagDelegates.Add(Tag, AbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UAbilityControlComponent::OnTagChanged));
	}
}

void UAbilityControlComponent::UnregisterTags()
{
	if (UAbilitySystemComponentBase* AbilitySystem = System.Get())
	{
		for (const auto& Entry : TagDelegates)
		{
			AbilitySystem->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::NewOrRemoved).Remove(Entry.Value);
		}
	}
	TagDelegates.Reset();
}

bool UAbilityControlComponent::IsActionBlocked(FGameplayTag ActionTag) const
{
	const UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FAbilityControlRule& Rule : Rules)
	{
		if (AbilitySystem->HasMatchingGameplayTag(Rule.Tag) && (Rule.bBlockAllActions || ActionTag.MatchesAny(Rule.BlockedActions)))
		{
			return true;
		}
	}
	return false;
}

bool UAbilityControlComponent::IsMovementBlocked() const
{
	const UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FAbilityControlRule& Rule : Rules)
	{
		if (Rule.bBlockMovement && AbilitySystem->HasMatchingGameplayTag(Rule.Tag))
		{
			return true;
		}
	}
	return false;
}

bool UAbilityControlComponent::IsRotationBlocked() const
{
	const UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem)
	{
		return false;
	}
	for (const FAbilityControlRule& Rule : Rules)
	{
		if (Rule.bBlockRotation && AbilitySystem->HasMatchingGameplayTag(Rule.Tag))
		{
			return true;
		}
	}
	return false;
}

void UAbilityControlComponent::OnTagChanged(FGameplayTag Tag, int32 Count)
{
	RefreshProjection();
}

bool UAbilityControlComponent::CanResumeMovement() const
{
	const IAbilityVitalityInterface* Vitality = Cast<IAbilityVitalityInterface>(GetOwner());
	return IsValid(GetOwner()) && Vitality && Vitality->IsActive() && !Vitality->IsDead();
}

void UAbilityControlComponent::RefreshProjection()
{
	UAbilitySystemComponentBase* AbilitySystem = System.Get();
	AActor* Owner = GetOwner();
	if (!AbilitySystem || AbilitySystem->GetAvatarActor() != Owner)
	{
		ReleaseMovement();
		return;
	}
	FCharacterMovementConstraint Constraint;
	for (const FAbilityControlRule& Rule : Rules)
	{
		if (AbilitySystem->HasMatchingGameplayTag(Rule.Tag))
		{
			Constraint.bBlockMovement |= Rule.bBlockMovement;
			Constraint.bBlockRotation |= Rule.bBlockRotation;
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponentBase* Movement = Cast<UCharacterMovementComponentBase>(Character->GetCharacterMovement()))
		{
			if (Constraint.bBlockMovement || Constraint.bBlockRotation)
			{
				Movement->SetMovementConstraint(MovementSource, this, Constraint);
			}
			else
			{
				Movement->ReleaseMovementSource(MovementSource, this);
			}
		}
	}
	else if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			if (Constraint.bBlockMovement)
			{
				if (!SuspendedMovement.IsValid())
				{
					bMovementWasActive = Movement->IsActive();
					SuspendedMovement = Movement;
				}
				Movement->StopMovementImmediately();
				Movement->SetActive(false);
				Pawn->ConsumeMovementInputVector();
			}
			else if (CanResumeMovement())
			{
				ReleaseMovement();
			}
		}
	}
	if (!AbilitySystem->IsOwnerActorAuthoritative())
	{
		return;
	}
	if (Constraint.bBlockMovement)
	{
		if (APawn* Pawn = Cast<APawn>(Owner))
		{
			if (AAIController* Controller = Cast<AAIController>(Pawn->GetController()))
			{
				Controller->StopMovement();
			}
		}
	}
	TArray<TPair<TWeakObjectPtr<UAbilityBase>, FGuid>> Cancellations;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (UAbilityBase* Action = Cast<UAbilityBase>(Spec.GetPrimaryInstance()))
		{
			if (Action->IsActive() && IsActionBlocked(Action->ActionTag))
			{
				Cancellations.Emplace(Action, Action->GetActionActivationId());
			}
		}
	}
	for (const auto& Cancellation : Cancellations)
	{
		UAbilityBase* Action = Cancellation.Key.Get();
		if (System.Get() == AbilitySystem && AbilitySystem->GetAvatarActor() == GetOwner() &&
			Action && Action->IsActive() && Action->GetActionActivationId() == Cancellation.Value && IsActionBlocked(Action->ActionTag))
		{
			Action->FailAction(AbilityActionTags::Blocked);
		}
	}
}

void UAbilityControlComponent::ReleaseMovement()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponentBase* Movement = Cast<UCharacterMovementComponentBase>(Character->GetCharacterMovement()))
		{
			Movement->ReleaseMovementSource(MovementSource, this);
		}
	}
	if (UPawnMovementComponent* Movement = SuspendedMovement.Get())
	{
		if (bMovementWasActive && CanResumeMovement())
		{
			Movement->SetActive(true);
		}
	}
	SuspendedMovement.Reset();
	bMovementWasActive = false;
}

void UAbilityControlComponent::SetStatic(bool bEnabled)
{
	UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative())
	{
		return;
	}
	if (!bEnabled)
	{
		AbilitySystem->RemoveActiveGameplayEffect(StaticHandle);
		StaticHandle.Invalidate();
		return;
	}
	if (!CanResumeMovement() || AbilitySystem->GetActiveGameplayEffect(StaticHandle))
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UAbilityControlEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->DynamicGrantedTags.AddTag(AbilityActionTags::StaticControl);
		StaticHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UAbilityControlComponent::AddInterrupt(float Duration)
{
	UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative() || !CanResumeMovement() || !FMath::IsFinite(Duration) || Duration <= 0.f)
	{
		return;
	}
	FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UAbilityTimedControlEffect::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetDuration(FMath::Clamp(Duration, 0.01f, 60.f), true);
		Spec.Data->DynamicGrantedTags.AddTag(GameplayTags::State_Vitality_Interrupting);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UAbilityControlComponent::ClearInterrupts()
{
	UAbilitySystemComponentBase* AbilitySystem = System.Get();
	if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative())
	{
		return;
	}
	FGameplayTagContainer Tags(GameplayTags::State_Vitality_Interrupting);
	for (const FActiveGameplayEffectHandle& Handle : AbilitySystem->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags)))
	{
		const FActiveGameplayEffect* Active = AbilitySystem->GetActiveGameplayEffect(Handle);
		if (Active && Active->Spec.Def && Active->Spec.Def->IsA<UAbilityControlEffect>())
		{
			AbilitySystem->RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UAbilityControlComponent::ResetControls()
{
	if (UAbilitySystemComponentBase* AbilitySystem = System.Get(); AbilitySystem && AbilitySystem->IsOwnerActorAuthoritative())
	{
		for (const FActiveGameplayEffectHandle& Handle : AbilitySystem->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Active = AbilitySystem->GetActiveGameplayEffect(Handle);
			if (Active && Active->Spec.Def && Active->Spec.Def->IsA<UAbilityControlEffect>())
			{
				AbilitySystem->RemoveActiveGameplayEffect(Handle);
			}
		}
	}
	StaticHandle.Invalidate();
}
