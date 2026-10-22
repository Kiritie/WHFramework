#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Inventory/InventoryAbilitySource.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemGlobals.h"
#include "GameplayAbilitySpec.h"
#include "GameplayCueManager.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Net/UnrealNetwork.h"
#include "Debug/DebugModuleTypes.h"
#include "GameplayEffectAggregator.h"
#include "Templates/UnrealTemplate.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "UObject/Package.h"
#include "TimerManager.h"

static TAutoConsoleVariable<float> CVarReplayMontageErrorThreshold(
	TEXT("GS.replay.MontageErrorThreshold"),
	0.5f,
	TEXT("Tolerance level for when montage playback position correction occurs in replays")
);

UAbilitySystemComponentBase::UAbilitySystemComponentBase()
{
	SetIsReplicatedByDefault(true);
}

void UAbilitySystemComponentBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAbilitySystemComponentBase, RepAnimMontageInfoForMeshes);
	DOREPLIFETIME_CONDITION(UAbilitySystemComponentBase, GrantedEntries, COND_OwnerOnly);
}

bool UAbilitySystemComponentBase::GetShouldTick() const
{
	if (bPendingMontageRep && AbilityActorInfo.IsValid() && AbilityActorInfo->AvatarActor.IsValid())
	{
		return true;
	}
	for (FGameplayAbilityRepAnimMontageForMesh RepMontageInfo : RepAnimMontageInfoForMeshes)
	{
		const bool bHasReplicatedMontageInfoToUpdate = (IsOwnerActorAuthoritative() && RepMontageInfo.RepMontageInfo.IsStopped == false);

		if (bHasReplicatedMontageInfoToUpdate)
		{
			return true;
		}
	}

	return Super::GetShouldTick();
}

void UAbilitySystemComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (IsOwnerActorAuthoritative())
	{
		for (FGameplayAbilityLocalAnimMontageForMesh& MontageInfo : LocalAnimMontageInfoForMeshes)
		{
			if (MontageInfo.bReplicateMontage)
			{
				AnimMontage_UpdateReplicatedDataForMesh(MontageInfo.Mesh);
			}
		}
	}

	if (!IsOwnerActorAuthoritative() && bPendingMontageRep)
	{
		OnRep_ReplicatedAnimMontageForMesh();
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAbilitySystemComponentBase::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	const bool bChangedAvatar = !AbilityActorInfo.IsValid() || AbilityActorInfo->AvatarActor.Get() != InAvatarActor;
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	if (bChangedAvatar)
	{
		LocalAnimMontageInfoForMeshes.Reset();
		if (IsOwnerActorAuthoritative())
		{
			RepAnimMontageInfoForMeshes.Reset();
		}
	}
	if (!IsOwnerActorAuthoritative() && RepAnimMontageInfoForMeshes.Num() > 0)
	{
		OnRep_ReplicatedAnimMontageForMesh();
	}
}

void UAbilitySystemComponentBase::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);

	// If AnimatingAbility ended, clear the pointer
	ClearAnimatingAbilityForAllMeshes(Ability);
}

void UAbilitySystemComponentBase::AbilityLocalInputPressed(int32 InputID)
{
	// Consume the input if this InputID is overloaded with GenericConfirm/Cancel and the GenericConfim/Cancel callback is bound
	if (IsGenericConfirmInputBound(InputID))
	{
		LocalInputConfirm();
		return;
	}

	if (IsGenericCancelInputBound(InputID))
	{
		LocalInputCancel();
		return;
	}

	// ---------------------------------------------------------

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.InputID == InputID)
		{
			if (Spec.Ability)
			{
				Spec.InputPressed = true;
				if (Spec.IsActive())
				{
					if (Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
					{
						ServerSetInputPressed(Spec.Handle);
					}

					AbilitySpecInputPressed(Spec);

					// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
				}
				else
				{
					UAbilityBase* GA = Cast<UAbilityBase>(Spec.Ability);
					if (GA && GA->bActivateOnInput)
					{
						const FAbilityGrantedEntry* Entry = GrantedEntries.FindByPredicate([&Spec](const FAbilityGrantedEntry& Candidate) { return Candidate.AbilityHandle == Spec.Handle; });
						if (Entry)
						{
							FAbilityActionRequest Request;
							Request.AbilityHandle = Entry->AbilityHandle;
							Request.DefinitionId = Entry->DefinitionId;
							Request.ActionTag = Entry->ActionTag;
							Request.TargetActor = GetAvatarActor();
							Request.TargetLocation = GetAvatarActor() ? GetAvatarActor()->GetActorLocation() : FVector::ZeroVector;
							RequestAction(Request);
						}
					}
				}
			}
		}
	}
}

FGameplayAbilitySpec UAbilitySystemComponentBase::FindAbilitySpecForHandle(FGameplayAbilitySpecHandle Handle)
{
	if(FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
	{
		return *Spec;
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec UAbilitySystemComponentBase::FindAbilitySpecForClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if(FGameplayAbilitySpec* Spec = FindAbilitySpecFromClass(AbilityClass))
	{
		return *Spec;
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec UAbilitySystemComponentBase::FindAbilitySpecForInputID(int32 InputID)
{
	if(FGameplayAbilitySpec* Spec = FindAbilitySpecFromInputID(InputID))
	{
		return *Spec;
	}
	return FGameplayAbilitySpec();
}


FGameplayAbilitySpecHandle UAbilitySystemComponentBase::FindAbilitySpecHandleForClass(TSubclassOf<UGameplayAbility> AbilityClass, UObject* OptionalSourceObject)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		TSubclassOf<UGameplayAbility> SpecAbilityClass = Spec.Ability->GetClass();
		if (SpecAbilityClass == AbilityClass)
		{
			if (!OptionalSourceObject || (OptionalSourceObject && Spec.SourceObject == OptionalSourceObject))
			{
				return Spec.Handle;
			}
		}
	}

	return FGameplayAbilitySpecHandle();
}

FAbilityInfo UAbilitySystemComponentBase::GetAbilityInfoBySpec(FGameplayAbilitySpec Spec)
{
	FAbilityInfo AbilityInfo;
	if(UAbilityBase* Ability = Cast<UAbilityBase>(Spec.Ability))
	{
		if(Ability->GetCostGameplayEffect() && Ability->GetCostGameplayEffect()->Modifiers.Num() > 0)
		{
			const FGameplayModifierInfo ModifierInfo = Ability->GetCostGameplayEffect()->Modifiers[0];
			AbilityInfo.CostAttribute = ModifierInfo.Attribute;
			ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, AbilityInfo.CostValue);
		}
		if(Ability->GetCooldownGameplayEffect())
		{
			Ability->GetCooldownGameplayEffect()->DurationMagnitude.GetStaticMagnitudeIfPossible(Spec.Level, AbilityInfo.CooldownDuration);
		}
	}
	if(UAbilityBase* Ability = Cast<UAbilityBase>(Spec.GetPrimaryInstance()))
	{
		AbilityInfo.CooldownRemaining = Ability->GetCooldownTimeRemaining();
	}
	return AbilityInfo;
}

FAbilityInfo UAbilitySystemComponentBase::GetAbilityInfoByHandle(FGameplayAbilitySpecHandle Handle)
{
	if(auto AbilitySpec = FindAbilitySpecFromHandle(Handle))
	{
		return GetAbilityInfoBySpec(*AbilitySpec);
	}
	return FAbilityInfo();
}

FAbilityInfo UAbilitySystemComponentBase::GetAbilityInfoByClass(TSubclassOf<UAbilityBase> AbilityClass)
{
	return GetAbilityInfoBySpec(FindAbilitySpecForClass(AbilityClass));
}

int32 UAbilitySystemComponentBase::K2_GetTagCount(FGameplayTag TagToCheck) const
{
	return GetTagCount(TagToCheck);
}

void UAbilitySystemComponentBase::K2_AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	AddLooseGameplayTag(GameplayTag, Count);
}

void UAbilitySystemComponentBase::K2_AddLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	AddLooseGameplayTags(GameplayTags, Count);
}

void UAbilitySystemComponentBase::K2_RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	RemoveLooseGameplayTag(GameplayTag, Count);
}

void UAbilitySystemComponentBase::K2_RemoveLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	RemoveLooseGameplayTags(GameplayTags, Count);
}

bool UAbilitySystemComponentBase::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately)
{
	const FAbilityGrantedEntry* Entry = GrantedEntries.FindByPredicate([InAbilityHandle](const FAbilityGrantedEntry& Candidate) { return Candidate.AbilityHandle == InAbilityHandle; });
	if (!Entry)
	{
		return false;
	}
	FAbilityActionRequest Request;
	Request.AbilityHandle = Entry->AbilityHandle;
	Request.DefinitionId = Entry->DefinitionId;
	Request.ActionTag = Entry->ActionTag;
	Request.TargetActor = GetAvatarActor();
	Request.TargetLocation = GetAvatarActor() ? GetAvatarActor()->GetActorLocation() : FVector::ZeroVector;
	const FAbilityActionStartResult Started = RequestAction(Request);
	if (Started.bAccepted && EndAbilityImmediately)
	{
		if (UAbilityBase* Action = FindActiveAction(Started.ActivationId))
		{
			Action->CompleteAction();
		}
	}
	return Started.bAccepted;
}

void UAbilitySystemComponentBase::ExecuteGameplayCueLocal(const FGameplayTag& GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::Executed, GameplayCueParameters);
}

void UAbilitySystemComponentBase::AddGameplayCueLocal(const FGameplayTag& GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::OnActive, GameplayCueParameters);
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::WhileActive, GameplayCueParameters);
}

void UAbilitySystemComponentBase::RemoveGameplayCueLocal(const FGameplayTag& GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::Removed, GameplayCueParameters);
}

FString UAbilitySystemComponentBase::GetCurrentPredictionKeyStatus()
{
	return ScopedPredictionKey.ToString() + " is valid for more prediction: " + (ScopedPredictionKey.IsValidForMorePrediction() ? TEXT("true") : TEXT("false"));

}

FActiveGameplayEffectHandle UAbilitySystemComponentBase::BP_ApplyGameplayEffectToSelfWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext)
{
	if (GameplayEffectClass)
	{
		if (!EffectContext.IsValid())
		{
			EffectContext = MakeEffectContext();
		}

		UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();

		if (CanPredict())
		{
			return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext, ScopedPredictionKey);
		}

		return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext);
	}

	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UAbilitySystemComponentBase::BP_ApplyGameplayEffectToTargetWithPrediction(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent* Target, float Level, FGameplayEffectContextHandle Context)
{
	if (Target == nullptr)
	{
		WHLog(FString::Printf(TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTargetWithPrediction called with null Target. %s. Context: %s"), *GetFullName(), *Context.ToString()), EDC_Ability, EDV_Warning);
		return FActiveGameplayEffectHandle();
	}

	if (GameplayEffectClass == nullptr)
	{
		WHLog(FString::Printf(TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTargetWithPrediction called with null GameplayEffectClass. %s. Context: %s"), *GetFullName(), *Context.ToString()), EDC_Ability, EDV_Warning);
		return FActiveGameplayEffectHandle();
	}

	UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();

	if (CanPredict())
	{
		return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context, ScopedPredictionKey);
	}

	return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context);
}

float UAbilitySystemComponentBase::PlayMontageForMesh(UGameplayAbility* InAnimatingAbility, USkeletalMeshComponent* InMesh, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName, bool bReplicateMontage)
{
	UAbilityBase* InAbility = Cast<UAbilityBase>(InAnimatingAbility);
	if (!InAbility || !InAbility->IsActive() || !AbilityActorInfo.IsValid() || (!IsOwnerActorAuthoritative() && !AbilityActorInfo->IsLocallyControlled()) || !FMath::IsFinite(InPlayRate) || InPlayRate <= 0.f || InPlayRate > 10.f)
	{
		return -1.f;
	}
	float Duration = -1.f;

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		const FGuid PlayingAction = InAbility->GetActionActivationId();
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (!InAbility->IsActive() || InAbility->GetActionActivationId() != PlayingAction || !AnimInstance->Montage_IsActive(NewAnimMontage))
		{
			return -1.f;
		}
		if (Duration > 0.f)
		{
			FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);

			if (AnimMontageInfo.LocalMontageInfo.AnimatingAbility.Get() && AnimMontageInfo.LocalMontageInfo.AnimatingAbility != InAnimatingAbility)
			{
				// The ability that was previously animating will have already gotten the 'interrupted' callback.
				// It may be a good idea to make this a global policy and 'cancel' the ability.
				//
				// For now, we expect it to end itself when this happens.
			}

			if (NewAnimMontage->HasRootMotion() && AnimInstance->GetOwningActor())
			{
				WHLog(FString::Printf(TEXT("UAbilitySystemComponent::PlayMontage %s, Role: %s")
					, *GetNameSafe(NewAnimMontage)
					, *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), AnimInstance->GetOwningActor()->GetLocalRole())
					), EDC_Ability);
			}

			AnimMontageInfo.LocalMontageInfo.AnimMontage = NewAnimMontage;
			AnimMontageInfo.LocalMontageInfo.AnimatingAbility = InAnimatingAbility;
			if (IsOwnerActorAuthoritative() && !bReplicateMontage)
			{
				if (FGameplayAbilityRepAnimMontageForMesh* Previous = RepAnimMontageInfoForMeshes.FindByPredicate([InMesh](const FGameplayAbilityRepAnimMontageForMesh& Entry) { return Entry.Mesh == InMesh; }))
				{
					Previous->RepMontageInfo.IsStopped = true;
					Previous->RepMontageInfo.BlendTime = 0.f;
					GetOwner()->ForceNetUpdate();
				}
			}
			AnimMontageInfo.bReplicateMontage = bReplicateMontage;
			++AnimMontageInfo.PlayInstanceId;
			if (AnimMontageInfo.PlayInstanceId == 0)
			{
				++AnimMontageInfo.PlayInstanceId;
			}

			if (InAbility)
			{
				InAbility->SetCurrentMontageForMesh(InMesh, NewAnimMontage);
			}

			// Start at a given Section.
			if (StartSectionName != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, NewAnimMontage);
			}

			// Replicate to non owners
			if (IsOwnerActorAuthoritative())
			{
				if (bReplicateMontage)
				{
					// Those are static parameters, they are only set when the montage is played. They are not changed after that.
					FGameplayAbilityRepAnimMontageForMesh& AbilityRepMontageInfo = GetGameplayAbilityRepAnimMontageForMesh(InMesh);
					AbilityRepMontageInfo.RepMontageInfo.Animation = NewAnimMontage;
					AnimMontage_UpdateForcedPlayFlagsForMesh(AbilityRepMontageInfo);

					// Update parameters that change during Montage life time.
					AnimMontage_UpdateReplicatedDataForMesh(InMesh);

					// Force net update on our avatar actor
					if (AbilityActorInfo->AvatarActor != nullptr)
					{
						GetOwner()->ForceNetUpdate();
					}
				}
			}
			else
			{
				// If this prediction key is rejected, we need to end the preview
				FPredictionKey PredictionKey = GetPredictionKeyForNewAction();
				if (PredictionKey.IsValidKey())
				{
					PredictionKey.NewRejectedDelegate().BindUObject(this, &UAbilitySystemComponentBase::OnPredictiveMontageRejectedForMesh, InMesh, NewAnimMontage, AnimMontageInfo.PlayInstanceId);
				}
			}
		}
	}

	return Duration;
}

float UAbilitySystemComponentBase::PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName)
{
	float Duration = -1.f;
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
			AnimMontageInfo.LocalMontageInfo.AnimMontage = NewAnimMontage;
		}
	}

	return Duration;
}

void UAbilitySystemComponentBase::CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	UAnimMontage* MontageToStop = AnimMontageInfo.LocalMontageInfo.AnimMontage;
	bool bShouldStopMontage = AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if (bShouldStopMontage)
	{
		const float BlendOutTime = (OverrideBlendOutTime >= 0.0f ? OverrideBlendOutTime : MontageToStop->BlendOut.GetBlendTime());

		AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);

		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedDataForMesh(InMesh);
		}
	}
}

void UAbilitySystemComponentBase::StopAllCurrentMontages(float OverrideBlendOutTime)
{
	for (FGameplayAbilityLocalAnimMontageForMesh& GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		CurrentMontageStopForMesh(GameplayAbilityLocalAnimMontageForMesh.Mesh, OverrideBlendOutTime);
	}
}

void UAbilitySystemComponentBase::StopMontageIfCurrentForMesh(USkeletalMeshComponent* InMesh, const UAnimMontage& Montage, float OverrideBlendOutTime)
{
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	if (&Montage == AnimMontageInfo.LocalMontageInfo.AnimMontage)
	{
		CurrentMontageStopForMesh(InMesh, OverrideBlendOutTime);
	}
}

void UAbilitySystemComponentBase::ClearAnimatingAbilityForAllMeshes(UGameplayAbility* Ability)
{
	UAbilityBase* AbilityBase = Cast<UAbilityBase>(Ability);
	for (FGameplayAbilityLocalAnimMontageForMesh& GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility == Ability)
		{
			if (AbilityBase)
			{
				AbilityBase->SetCurrentMontageForMesh(GameplayAbilityLocalAnimMontageForMesh.Mesh, nullptr);
			}
			GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility = nullptr;
		}
	}
}

void UAbilitySystemComponentBase::CurrentMontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	if ((SectionName != NAME_None) && AnimInstance && AnimMontageInfo.LocalMontageInfo.AnimMontage)
	{
		AnimInstance->Montage_JumpToSection(SectionName, AnimMontageInfo.LocalMontageInfo.AnimMontage);
		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedDataForMesh(InMesh);
		}

	}
}

void UAbilitySystemComponentBase::CurrentMontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	if (AnimMontageInfo.LocalMontageInfo.AnimMontage && AnimInstance)
	{
		// Set Next Section Name.
		AnimInstance->Montage_SetNextSection(FromSectionName, ToSectionName, AnimMontageInfo.LocalMontageInfo.AnimMontage);

		// Update replicated version for Simulated Proxies if we are on the server.
		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedDataForMesh(InMesh);
		}

	}
}

void UAbilitySystemComponentBase::CurrentMontageSetPlayRateForMesh(USkeletalMeshComponent* InMesh, float InPlayRate)
{
	if (!FMath::IsFinite(InPlayRate) || InPlayRate < 0.f || InPlayRate > 10.f)
	{
		return;
	}
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	if (AnimMontageInfo.LocalMontageInfo.AnimMontage && AnimInstance)
	{
		// Set Play Rate
		AnimInstance->Montage_SetPlayRate(AnimMontageInfo.LocalMontageInfo.AnimMontage, InPlayRate);

		// Update replicated version for Simulated Proxies if we are on the server.
		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedDataForMesh(InMesh);
		}

	}
}

bool UAbilitySystemComponentBase::IsAnimatingAbilityForAnyMesh(UGameplayAbility* InAbility) const
{
	for (FGameplayAbilityLocalAnimMontageForMesh GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility == InAbility)
		{
			return true;
		}
	}

	return false;
}

UGameplayAbility* UAbilitySystemComponentBase::GetAnimatingAbilityFromAnyMesh()
{
	// Only one ability can be animating for all meshes
	for (FGameplayAbilityLocalAnimMontageForMesh& GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility.Get())
		{
			return GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility.Get();
		}
	}

	return nullptr;
}

TArray<UAnimMontage*> UAbilitySystemComponentBase::GetCurrentMontages() const
{
	TArray<UAnimMontage*> Montages;

	for (FGameplayAbilityLocalAnimMontageForMesh GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(GameplayAbilityLocalAnimMontageForMesh.Mesh)
			&& GameplayAbilityLocalAnimMontageForMesh.Mesh->GetOwner() == AbilityActorInfo->AvatarActor ? GameplayAbilityLocalAnimMontageForMesh.Mesh->GetAnimInstance() : nullptr;

		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimMontage && AnimInstance
			&& AnimInstance->Montage_IsActive(GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimMontage))
		{
			Montages.Add(GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimMontage);
		}
	}

	return Montages;
}

UAnimMontage* UAbilitySystemComponentBase::GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	const FGameplayAbilityLocalAnimMontageForMesh* Entry = LocalAnimMontageInfoForMeshes.FindByPredicate([InMesh](const FGameplayAbilityLocalAnimMontageForMesh& Value) { return Value.Mesh == InMesh; });
	return Entry && Entry->LocalMontageInfo.AnimMontage && AnimInstance && AnimInstance->Montage_IsActive(Entry->LocalMontageInfo.AnimMontage) ? Entry->LocalMontageInfo.AnimMontage.Get() : nullptr;
}

int32 UAbilitySystemComponentBase::GetCurrentMontageSectionIDForMesh(USkeletalMeshComponent* InMesh)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontageForMesh(InMesh);

	if (CurrentAnimMontage && AnimInstance)
	{
		float MontagePosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		return CurrentAnimMontage->GetSectionIndexFromPosition(MontagePosition);
	}

	return INDEX_NONE;
}

FName UAbilitySystemComponentBase::GetCurrentMontageSectionNameForMesh(USkeletalMeshComponent* InMesh)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontageForMesh(InMesh);

	if (CurrentAnimMontage && AnimInstance)
	{
		float MontagePosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		int32 CurrentSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(MontagePosition);

		return CurrentAnimMontage->GetSectionName(CurrentSectionID);
	}

	return NAME_None;
}

float UAbilitySystemComponentBase::GetCurrentMontageSectionLengthForMesh(USkeletalMeshComponent* InMesh)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontageForMesh(InMesh);

	if (CurrentAnimMontage && AnimInstance)
	{
		int32 CurrentSectionID = GetCurrentMontageSectionIDForMesh(InMesh);
		if (CurrentSectionID != INDEX_NONE)
		{
			TArray<FCompositeSection>& CompositeSections = CurrentAnimMontage->CompositeSections;

			// If we have another section after us, then take delta between both start times.
			if (CurrentSectionID < (CompositeSections.Num() - 1))
			{
				return (CompositeSections[CurrentSectionID + 1].GetTime() - CompositeSections[CurrentSectionID].GetTime());
			}
			// Otherwise we are the last section, so take delta with Montage total time.
			else
			{
				return (CurrentAnimMontage->GetPlayLength() - CompositeSections[CurrentSectionID].GetTime());
			}
		}

		// if we have no sections, just return total length of Montage.
		return CurrentAnimMontage->GetPlayLength();
	}

	return 0.f;
}

float UAbilitySystemComponentBase::GetCurrentMontageSectionTimeLeftForMesh(USkeletalMeshComponent* InMesh)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentAnimMontage = GetCurrentMontageForMesh(InMesh);

	if (CurrentAnimMontage && AnimInstance && AnimInstance->Montage_IsActive(CurrentAnimMontage))
	{
		const float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		return CurrentAnimMontage->GetSectionTimeLeftFromPos(CurrentPosition);
	}

	return -1.f;
}

FGameplayAbilityLocalAnimMontageForMesh& UAbilitySystemComponentBase::GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* InMesh)
{
	for (FGameplayAbilityLocalAnimMontageForMesh& MontageInfo : LocalAnimMontageInfoForMeshes)
	{
		if (MontageInfo.Mesh == InMesh)
		{
			return MontageInfo;
		}
	}

	FGameplayAbilityLocalAnimMontageForMesh MontageInfo = FGameplayAbilityLocalAnimMontageForMesh(InMesh);
	LocalAnimMontageInfoForMeshes.Add(MontageInfo);
	return LocalAnimMontageInfoForMeshes.Last();
}

FGameplayAbilityRepAnimMontageForMesh& UAbilitySystemComponentBase::GetGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh)
{
	for (FGameplayAbilityRepAnimMontageForMesh& RepMontageInfo : RepAnimMontageInfoForMeshes)
	{
		if (RepMontageInfo.Mesh == InMesh)
		{
			return RepMontageInfo;
		}
	}

	FGameplayAbilityRepAnimMontageForMesh RepMontageInfo = FGameplayAbilityRepAnimMontageForMesh(InMesh);
	RepAnimMontageInfoForMeshes.Add(RepMontageInfo);
	return RepAnimMontageInfoForMeshes.Last();
}

void UAbilitySystemComponentBase::OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage, uint32 PlayInstanceId)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	const FGameplayAbilityLocalAnimMontageForMesh* Entry = LocalAnimMontageInfoForMeshes.FindByPredicate([InMesh](const FGameplayAbilityLocalAnimMontageForMesh& Value) { return Value.Mesh == InMesh; });
	if (AnimInstance && Entry && Entry->PlayInstanceId == PlayInstanceId && Entry->LocalMontageInfo.AnimMontage == PredictiveMontage && AnimInstance->Montage_IsPlaying(PredictiveMontage))
	{
		AnimInstance->Montage_Stop(0.25f, PredictiveMontage);
	}
}

void UAbilitySystemComponentBase::AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh)
{
	if (!IsOwnerActorAuthoritative() || !IsValid(InMesh))
	{
		return;
	}
	const FGameplayAbilityLocalAnimMontageForMesh* Entry = LocalAnimMontageInfoForMeshes.FindByPredicate([InMesh](const FGameplayAbilityLocalAnimMontageForMesh& Value) { return Value.Mesh == InMesh; });
	if (Entry && Entry->bReplicateMontage)
	{
		AnimMontage_UpdateReplicatedDataForMesh(GetGameplayAbilityRepAnimMontageForMesh(InMesh));
	}
}

void UAbilitySystemComponentBase::AnimMontage_UpdateReplicatedDataForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() && IsValid(OutRepAnimMontageInfo.Mesh) && OutRepAnimMontageInfo.Mesh->GetOwner()
		== AbilityActorInfo->AvatarActor ? OutRepAnimMontageInfo.Mesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(OutRepAnimMontageInfo.Mesh);

	if (AnimInstance && AnimMontageInfo.LocalMontageInfo.AnimMontage)
	{
		OutRepAnimMontageInfo.RepMontageInfo.Animation = AnimMontageInfo.LocalMontageInfo.AnimMontage;

		// Compressed Flags
		bool bIsStopped = AnimInstance->Montage_GetIsStopped(AnimMontageInfo.LocalMontageInfo.AnimMontage);

		if (!bIsStopped)
		{
			OutRepAnimMontageInfo.RepMontageInfo.PlayRate = AnimInstance->Montage_GetPlayRate(AnimMontageInfo.LocalMontageInfo.AnimMontage);
			OutRepAnimMontageInfo.RepMontageInfo.Position = AnimInstance->Montage_GetPosition(AnimMontageInfo.LocalMontageInfo.AnimMontage);
			OutRepAnimMontageInfo.RepMontageInfo.BlendTime = AnimInstance->Montage_GetBlendTime(AnimMontageInfo.LocalMontageInfo.AnimMontage);
		}

		if (OutRepAnimMontageInfo.RepMontageInfo.IsStopped != bIsStopped)
		{
			// Set this prior to calling UpdateShouldTick, so we start ticking if we are playing a Montage
			OutRepAnimMontageInfo.RepMontageInfo.IsStopped = bIsStopped;

			// When we start or stop an animation, update the clients right away for the Avatar Actor
			if (AbilityActorInfo->AvatarActor != nullptr)
			{
				GetOwner()->ForceNetUpdate();
			}

			// When this changes, we should update whether or not we should be ticking
			UpdateShouldTick();
		}

		// Replicate NextSectionID to keep it in sync.
		// We actually replicate NextSectionID+1 on a BYTE to put INDEX_NONE in there.
		int32 CurrentSectionID = AnimMontageInfo.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(OutRepAnimMontageInfo.RepMontageInfo.Position);
		if (CurrentSectionID != INDEX_NONE)
		{
			int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(AnimMontageInfo.LocalMontageInfo.AnimMontage, CurrentSectionID);
			if (NextSectionID >= (256 - 1))
			{
				WHLog(FString::Printf(TEXT("AnimMontage_UpdateReplicatedData. NextSectionID = %d.  RepAnimMontageInfo.Position: %.2f, CurrentSectionID: %d. LocalAnimMontageInfo.AnimMontage %s"),
					NextSectionID, OutRepAnimMontageInfo.RepMontageInfo.Position, CurrentSectionID, *GetNameSafe(AnimMontageInfo.LocalMontageInfo.AnimMontage)), EDC_Ability, EDV_Warning);
				ensure(NextSectionID < (256 - 1));
			}
			OutRepAnimMontageInfo.RepMontageInfo.NextSectionID = uint8(NextSectionID + 1);
		}
		else
		{
			OutRepAnimMontageInfo.RepMontageInfo.NextSectionID = 0;
		}
	}
}

void UAbilitySystemComponentBase::AnimMontage_UpdateForcedPlayFlagsForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo)
{
	const FGameplayAbilityLocalAnimMontageForMesh& Local = GetLocalAnimMontageInfoForMesh(OutRepAnimMontageInfo.Mesh);
	OutRepAnimMontageInfo.PlayInstanceId = Local.PlayInstanceId;
}

void UAbilitySystemComponentBase::OnRep_ReplicatedAnimMontageForMesh()
{
	bPendingMontageRep = false;
	if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->AvatarActor.IsValid())
	{
		bPendingMontageRep = true;
		return;
	}
	for (FGameplayAbilityRepAnimMontageForMesh& NewRepMontageInfoForMesh : RepAnimMontageInfoForMeshes)
	{
		if (IsValid(NewRepMontageInfoForMesh.Mesh) && NewRepMontageInfoForMesh.Mesh->GetOwner() != AbilityActorInfo->AvatarActor.Get())
		{
			continue;
		}
		FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(NewRepMontageInfoForMesh.Mesh);

		UWorld* World = GetWorld();

		if (NewRepMontageInfoForMesh.RepMontageInfo.bSkipPlayRate)
		{
			NewRepMontageInfoForMesh.RepMontageInfo.PlayRate = 1.f;
		}

		const bool bIsPlayingReplay = World && World->IsPlayingReplay();

		const float MONTAGE_REP_POS_ERR_THRESH = bIsPlayingReplay ? CVarReplayMontageErrorThreshold.GetValueOnGameThread() : 0.1f;

		UAnimInstance* AnimInstance = IsValid(NewRepMontageInfoForMesh.Mesh) && NewRepMontageInfoForMesh.Mesh->GetOwner()
			== AbilityActorInfo->AvatarActor ? NewRepMontageInfoForMesh.Mesh->GetAnimInstance() : nullptr;
		if (AnimInstance == nullptr || !IsReadyForReplicatedMontageForMesh())
		{
			// We can't handle this yet
			bPendingMontageRep = true;
			continue;
		}

		if (!AbilityActorInfo->IsLocallyControlled())
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.Montage.Debug"));
			bool DebugMontage = (CVar && CVar->GetValueOnGameThread() == 1);
			if (DebugMontage)
			{
				WHLog(FString::Printf(TEXT("\n\nOnRep_ReplicatedAnimMontage, %s"), *GetNameSafe(this)), EDC_Ability);
				WHLog(FString::Printf(TEXT("\tAnimMontage: %s\n\tPlayRate: %f\n\tPosition: %f\n\tBlendTime: %f\n\tNextSectionID: %d\n\tIsStopped: %d"),
					*GetNameSafe(NewRepMontageInfoForMesh.RepMontageInfo.GetAnimMontage()),
					NewRepMontageInfoForMesh.RepMontageInfo.PlayRate,
					NewRepMontageInfoForMesh.RepMontageInfo.Position,
					NewRepMontageInfoForMesh.RepMontageInfo.BlendTime,
					NewRepMontageInfoForMesh.RepMontageInfo.NextSectionID,
					NewRepMontageInfoForMesh.RepMontageInfo.IsStopped), EDC_Ability);
				WHLog(FString::Printf(TEXT("\tLocalAnimMontageInfo.AnimMontage: %s\n\tPosition: %f"),
					*GetNameSafe(AnimMontageInfo.LocalMontageInfo.AnimMontage), AnimInstance->Montage_GetPosition(AnimMontageInfo.LocalMontageInfo.AnimMontage)), EDC_Ability);
			}

			if (NewRepMontageInfoForMesh.RepMontageInfo.GetAnimMontage())
			{
				// New Montage to play
				if (AnimMontageInfo.LocalMontageInfo.AnimMontage != NewRepMontageInfoForMesh.RepMontageInfo.GetAnimMontage() || AnimMontageInfo.PlayInstanceId != NewRepMontageInfoForMesh.PlayInstanceId)
				{
					PlayMontageSimulatedForMesh(NewRepMontageInfoForMesh.Mesh, NewRepMontageInfoForMesh.RepMontageInfo.GetAnimMontage(), NewRepMontageInfoForMesh.RepMontageInfo.PlayRate);
					AnimMontageInfo.PlayInstanceId = NewRepMontageInfoForMesh.PlayInstanceId;
				}

				if (AnimMontageInfo.LocalMontageInfo.AnimMontage == nullptr)
				{
					WHLog(FString::Printf(TEXT("OnRep_ReplicatedAnimMontage: PlayMontageSimulated failed. Name: %s, AnimMontage: %s"), *GetNameSafe(this), *GetNameSafe(NewRepMontageInfoForMesh.RepMontageInfo.GetAnimMontage())), EDC_Ability, EDV_Warning);
					return;
				}

				// Play Rate has changed
				if (AnimInstance->Montage_GetPlayRate(AnimMontageInfo.LocalMontageInfo.AnimMontage) != NewRepMontageInfoForMesh.RepMontageInfo.PlayRate)
				{
					AnimInstance->Montage_SetPlayRate(AnimMontageInfo.LocalMontageInfo.AnimMontage, NewRepMontageInfoForMesh.RepMontageInfo.PlayRate);
				}

				// Compressed Flags
				const bool bIsStopped = AnimInstance->Montage_GetIsStopped(AnimMontageInfo.LocalMontageInfo.AnimMontage);
				const bool bReplicatedIsStopped = bool(NewRepMontageInfoForMesh.RepMontageInfo.IsStopped);

				// Process stopping first, so we don't change sections and cause blending to pop.
				if (bReplicatedIsStopped)
				{
					if (!bIsStopped)
					{
						CurrentMontageStopForMesh(NewRepMontageInfoForMesh.Mesh, NewRepMontageInfoForMesh.RepMontageInfo.BlendTime);
					}
				}
				else if (!NewRepMontageInfoForMesh.RepMontageInfo.SkipPositionCorrection)
				{
					const int32 RepSectionID = AnimMontageInfo.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(NewRepMontageInfoForMesh.RepMontageInfo.Position);
					const int32 RepNextSectionID = int32(NewRepMontageInfoForMesh.RepMontageInfo.NextSectionID) - 1;

					// And NextSectionID for the replicated SectionID.
					if (RepSectionID != INDEX_NONE)
					{
						const int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(AnimMontageInfo.LocalMontageInfo.AnimMontage, RepSectionID);

						// If NextSectionID is different than the replicated one, then set it.
						if (NextSectionID != RepNextSectionID)
						{
							AnimInstance->Montage_SetNextSection(AnimMontageInfo.LocalMontageInfo.AnimMontage->GetSectionName(RepSectionID), AnimMontageInfo.LocalMontageInfo.AnimMontage->GetSectionName(RepNextSectionID), AnimMontageInfo.LocalMontageInfo.AnimMontage);
						}

						// Make sure we haven't received that update too late and the client hasn't already jumped to another section.
						const int32 CurrentSectionID = AnimMontageInfo.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(AnimInstance->Montage_GetPosition(AnimMontageInfo.LocalMontageInfo.AnimMontage));
						if ((CurrentSectionID != RepSectionID) && (CurrentSectionID != RepNextSectionID))
						{
							// Client is in a wrong section, teleport him into the begining of the right section
							const float SectionStartTime = AnimMontageInfo.LocalMontageInfo.AnimMontage->GetAnimCompositeSection(RepSectionID).GetTime();
							AnimInstance->Montage_SetPosition(AnimMontageInfo.LocalMontageInfo.AnimMontage, SectionStartTime);
						}
					}

					// Update Position. If error is too great, jump to replicated position.
					const float CurrentPosition = AnimInstance->Montage_GetPosition(AnimMontageInfo.LocalMontageInfo.AnimMontage);
					const int32 CurrentSectionID = AnimMontageInfo.LocalMontageInfo.AnimMontage->GetSectionIndexFromPosition(CurrentPosition);
					const float DeltaPosition = NewRepMontageInfoForMesh.RepMontageInfo.Position - CurrentPosition;

					// Only check threshold if we are located in the same section. Different sections require a bit more work as we could be jumping around the timeline.
					// And therefore DeltaPosition is not as trivial to determine.
					if ((CurrentSectionID == RepSectionID) && (FMath::Abs(DeltaPosition) > MONTAGE_REP_POS_ERR_THRESH) && (NewRepMontageInfoForMesh.RepMontageInfo.IsStopped == 0))
					{
						// fast forward to server position and trigger notifies
						if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(NewRepMontageInfoForMesh.RepMontageInfo.GetAnimMontage()))
						{
							// Skip triggering notifies if we're going backwards in time, we've already triggered them.
							const float DeltaTime = !FMath::IsNearlyZero(NewRepMontageInfoForMesh.RepMontageInfo.PlayRate) ? (DeltaPosition / NewRepMontageInfoForMesh.RepMontageInfo.PlayRate) : 0.f;
							if (DeltaTime >= 0.f)
							{
								MontageInstance->UpdateWeight(DeltaTime);
								MontageInstance->HandleEvents(CurrentPosition, NewRepMontageInfoForMesh.RepMontageInfo.Position, nullptr);
								AnimInstance->TriggerAnimNotifies(DeltaTime);
							}
						}
						AnimInstance->Montage_SetPosition(AnimMontageInfo.LocalMontageInfo.AnimMontage, NewRepMontageInfoForMesh.RepMontageInfo.Position);
					}
				}
			}
		}
	}
}

bool UAbilitySystemComponentBase::IsReadyForReplicatedMontageForMesh()
{
	/** Children may want to override this for additional checks (e.g, "has skin been applied") */
	return true;
}













void UAbilitySystemComponentBase::GetActiveAbilitiesWithTags(const FGameplayTagContainer& AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	ActiveAbilities.Reset();
	TArray<FGameplayAbilitySpec*> Specs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTags, Specs, false);
	for (FGameplayAbilitySpec* Spec : Specs)
	{
		if (!Spec || !Spec->IsActive())
		{
			continue;
		}
		for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
		{
			if (UAbilityBase* Action = Cast<UAbilityBase>(Instance); Action && Action->IsActive())
			{
				ActiveAbilities.AddUnique(Action);
			}
		}
	}
}

int32 UAbilitySystemComponentBase::GetDefaultAbilityLevel() const
{
	AAbilityCharacterBase* OwnerCharacter = Cast<AAbilityCharacterBase>(GetAvatarActor());
	if (OwnerCharacter)
	{
		return OwnerCharacter->GetLevelA();
	}
	return 1;
}

UAbilitySystemComponentBase* UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(const AActor* Actor, bool LookForComponent /*= false*/)
{
	return Cast<UAbilitySystemComponentBase>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, LookForComponent));
}


FAbilityActionStartResult UAbilitySystemComponentBase::RequestAction(const FAbilityActionRequest& Request)
{
	FAbilityActionStartResult Start;
	Start.ActivationId = FGuid::NewGuid();
	FGameplayTag Reason;
	if (PendingActionReplies.Num() >= 64 || bGrantMutation || bAvatarUnbinding || !AbilityActorInfo.IsValid() || !AbilityActorInfo->AvatarActor.IsValid() || (!IsOwnerActorAuthoritative() && !AbilityActorInfo->IsLocallyControlled()))
	{
		Reason = AbilityActionTags::NotReady;
	}
	else if (!ValidateGrantedRequest(Request, Reason))
	{
	}
	else if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Request.AbilityHandle))
	{
		if (Spec->IsActive() || PendingActionReplies.FindKey(Request.AbilityHandle))
		{
			Reason = AbilityActionTags::AlreadyActive;
		}
	}
	if (Reason.IsValid())
	{
		FAbilityActionResult Result;
		Result.ActivationId = Start.ActivationId;
		Result.AbilityHandle = Request.AbilityHandle;
		Result.Outcome = EAbilityActionOutcome::Rejected;
		Result.Reason = Reason;
		PublishActionResult(Result);
		return Start;
	}
	FAbilityActionTargetData* Data = new FAbilityActionTargetData();
	Data->ActivationId = Start.ActivationId;
	Data->Request = Request;
	Data->Request.AimDirection = Request.AimDirection.GetSafeNormal();
	Data->Request.MovementDirection = Request.MovementDirection.GetSafeNormal();
	FGameplayEventData Event;
	Event.EventTag = AbilityActionTags::Request;
	Event.Instigator = AbilityActorInfo->AvatarActor.Get();
	Event.Target = Request.TargetActor;
	Event.TargetData.Add(Data);
	PendingActionReplies.Add(Start.ActivationId, Request.AbilityHandle);
	TGuardValue<FGuid> DispatchId(DispatchedActionId, Start.ActivationId);
	TGuardValue<FGameplayAbilitySpecHandle> DispatchHandle(DispatchedAbilityHandle, Request.AbilityHandle);
	const FGameplayAbilitySpec* RequestedSpec = FindAbilitySpecFromHandle(Request.AbilityHandle);
	const UGameplayAbility* Definition = RequestedSpec ? RequestedSpec->Ability.Get() : nullptr;
	if (!IsOwnerActorAuthoritative() && Definition && (Definition->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerInitiated || Definition->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly))
	{
		ServerTryActivateAbilityWithEventData(Request.AbilityHandle, false, FPredictionKey(), Event);
		Start.bAccepted = true;
	}
	else
	{
		Start.bAccepted = TriggerAbilityFromGameplayEvent(Request.AbilityHandle, AbilityActorInfo.Get(), AbilityActionTags::Request, &Event, *this);
	}
	if (!Start.bAccepted && !RecentActionResults.Contains(Start.ActivationId))
	{
		FAbilityActionResult Result;
		Result.ActivationId = Start.ActivationId;
		Result.AbilityHandle = Request.AbilityHandle;
		Result.Outcome = EAbilityActionOutcome::Rejected;
		Result.Reason = AbilityActionTags::ActivationFailed;
		PublishActionResult(Result);
	}
	if (const FAbilityActionResult* Immediate = RecentActionResults.Find(Start.ActivationId))
	{
		Start.bAccepted = Immediate->Outcome != EAbilityActionOutcome::Rejected;
	}
	return Start;
}

bool UAbilitySystemComponentBase::ValidateGrantedRequest(const FAbilityActionRequest& Request, FGameplayTag& OutReason, bool bRequireReplicatedEntry) const
{

	for (const FAbilityGrantedEntry& Entry : GrantedEntries)
	{
		if (Entry.AbilityHandle == Request.AbilityHandle && (!Entry.bActionEnabled || SuspendedGrantSources.Contains(Entry.SourceId)))
		{
			OutReason = AbilityActionTags::SourceRemoved;
			return false;
		}
	}
	if (bGrantMutation || bAvatarUnbinding)
	{
		OutReason = AbilityActionTags::NotReady;
		return false;
	}
	if (!Request.AbilityHandle.IsValid() || !Request.DefinitionId.IsValid() || !Request.ActionTag.IsValid() || Request.TargetLocation.ContainsNaN() || Request.AimDirection.ContainsNaN() || Request.MovementDirection.ContainsNaN() || Request.MovementDirection.SizeSquared() > 1.01f || Request.AimDirection.IsNearlyZero() || Request.WeaponPart < 0 || Request.WeaponPart > 255 || Request.ComboIndex < INDEX_NONE || Request.ComboIndex > 255)
	{
		OutReason = AbilityActionTags::InvalidRequest;
		return false;
	}
	const FAbilityGrantedEntry* Entry = GrantedEntries.FindByPredicate([&Request](const FAbilityGrantedEntry& Candidate)
	{
		return Candidate.AbilityHandle == Request.AbilityHandle && Candidate.DefinitionId == Request.DefinitionId && Candidate.ActionTag == Request.ActionTag;
	});
	const FGameplayAbilitySpec* Spec = const_cast<UAbilitySystemComponentBase*>(this)->FindAbilitySpecFromHandle(Request.AbilityHandle);
	const UAbilityBase* Definition = Spec ? Cast<UAbilityBase>(Spec->Ability) : nullptr;
	if (!Definition || Definition->ActionTag != Request.ActionTag || ((bRequireReplicatedEntry || IsOwnerActorAuthoritative()) && !Entry))
	{
		OutReason = AbilityActionTags::NotGranted;
		return false;
	}
	if (IsOwnerActorAuthoritative())
	{
		const FAbilityGrantSourceState* Source = GrantSources.Find(Entry->SourceId);
		const int32 Index = Source ? Source->AbilityHandles.IndexOfByKey(Request.AbilityHandle) : INDEX_NONE;
		if (!Source || !Source->Abilities.IsValidIndex(Index) || Source->Abilities[Index].DefinitionId != Request.DefinitionId || Source->Abilities[Index].AbilityClass != Definition->GetClass() || Source->Abilities[Index].SourceObject != Spec->SourceObject.Get())
		{
			OutReason = AbilityActionTags::NotGranted;
			return false;
		}
	}
	return true;
}

void UAbilitySystemComponentBase::InternalServerTryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed, const FPredictionKey& PredictionKey, const FGameplayEventData* TriggerEventData)
{
	FGuid ActivationId;
	FAbilityActionRequest Request;
	const bool bHasRequest = FAbilityActionTargetData::Read(TriggerEventData, ActivationId, Request);
	const FGameplayAbilitySpec* OwnedSpec = FindAbilitySpecFromHandle(AbilityToActivate);
	const bool bAlreadyActive = OwnedSpec && OwnedSpec->IsActive();
	Request.AbilityHandle = AbilityToActivate;
	FGameplayTag Reason;
	if (!bHasRequest || bAlreadyActive || bAvatarUnbinding || !AbilityActorInfo.IsValid() || !AbilityActorInfo->AvatarActor.IsValid() || TriggerEventData->Instigator.Get() != AbilityActorInfo->AvatarActor.Get() || !ValidateGrantedRequest(Request, Reason) || RecentActionResults.Contains(ActivationId) || IsActionActive(ActivationId))
	{
		ClientActivateAbilityFailed(AbilityToActivate, PredictionKey.Current);
		if (bHasRequest && !RecentActionResults.Contains(ActivationId) && !IsActionActive(ActivationId))
		{
			FAbilityActionResult Result;
			Result.ActivationId = ActivationId;
			Result.AbilityHandle = AbilityToActivate;
			Result.Outcome = EAbilityActionOutcome::Rejected;
			Result.Reason = bAlreadyActive ? AbilityActionTags::AlreadyActive : (Reason.IsValid() ? Reason : AbilityActionTags::InvalidRequest);
			PublishActionResult(Result);
		}
		return;
	}
	TGuardValue<FGuid> DispatchId(DispatchedActionId, ActivationId);
	TGuardValue<FGameplayAbilitySpecHandle> DispatchHandle(DispatchedAbilityHandle, AbilityToActivate);
	PendingActionReplies.Add(ActivationId, AbilityToActivate);
	Super::InternalServerTryActivateAbility(AbilityToActivate, InputPressed, PredictionKey, TriggerEventData);
	if (!IsActionActive(ActivationId) && !RecentActionResults.Contains(ActivationId))
	{
		FAbilityActionResult Result;
		Result.ActivationId = ActivationId;
		Result.AbilityHandle = AbilityToActivate;
		Result.Outcome = EAbilityActionOutcome::Rejected;
		Result.Reason = AbilityActionTags::ActivationFailed;
		PublishActionResult(Result);
	}
}

void UAbilitySystemComponentBase::NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	Super::NotifyAbilityFailed(Handle, Ability, FailureReason);
	if (DispatchedActionId.IsValid() && Handle == DispatchedAbilityHandle)
	{
		FAbilityActionResult Result;
		Result.ActivationId = DispatchedActionId;
		Result.AbilityHandle = Handle;
		Result.Outcome = EAbilityActionOutcome::Rejected;
		Result.Reason = AbilityActionTags::ActivationFailed;
		Result.FailureTags = FailureReason;
		PublishActionResult(Result);
	}
}

UAbilityBase* UAbilitySystemComponentBase::FindActiveAction(const FGuid& ActivationId) const
{
	if (!ActivationId.IsValid())
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			UAbilityBase* Action = Cast<UAbilityBase>(Instance);
			if (Action && Action->IsActive() && Action->GetActionActivationId() == ActivationId)
			{
				return Action;
			}
		}
	}
	return nullptr;
}

bool UAbilitySystemComponentBase::CancelAction(const FGuid& ActivationId, FGameplayTag Reason)
{
	if (!IsOwnerActorAuthoritative())
	{
		if (!AbilityActorInfo.IsValid() || !AbilityActorInfo->IsLocallyControlled())
		{
			return false;
		}
		if (const FGameplayAbilitySpecHandle* Handle = PendingActionReplies.Find(ActivationId))
		{
			ServerCancelRequestedAction(ActivationId, *Handle, Reason);
		}
		return false;
	}
	if (UAbilityBase* Action = FindActiveAction(ActivationId))
	{
		return Action->RequestActionCancellation(Reason);
	}
	return false;
}

void UAbilitySystemComponentBase::ServerCancelRequestedAction_Implementation(FGuid ActivationId, FGameplayAbilitySpecHandle Handle, FGameplayTag Reason)
{
	if (!ActivationId.IsValid() || !Handle.IsValid())
	{
		return;
	}
	if (UAbilityBase* Action = FindActiveAction(ActivationId); Action && Action->GetActionRequest().AbilityHandle == Handle)
	{
		Action->RequestActionCancellation(AbilityActionTags::Cancelled);
	}
	else if (const FAbilityActionResult* Result = RecentActionResults.Find(ActivationId); Result && Result->AbilityHandle == Handle)
	{
		ClientReceiveActionResult(*Result);
	}
}

bool UAbilitySystemComponentBase::TrackActionActivation(const FGuid& ActivationId, FGameplayAbilitySpecHandle Handle)
{
	if (!ActivationId.IsValid() || !Handle.IsValid() || bAvatarUnbinding || RecentActionResults.Contains(ActivationId))
	{
		return false;
	}
	if (const FGameplayAbilitySpecHandle* Existing = PendingActionReplies.Find(ActivationId); Existing && *Existing != Handle)
	{
		return false;
	}
	PendingActionReplies.Add(ActivationId, Handle);
	return true;
}

bool UAbilitySystemComponentBase::IsActionActive(const FGuid& ActivationId) const
{
	return FindActiveAction(ActivationId) != nullptr;
}

FGameplayAbilitySpecHandle UAbilitySystemComponentBase::GetPendingActionHandle(const FGuid& ActivationId) const
{
	const FGameplayAbilitySpecHandle* Handle = PendingActionReplies.Find(ActivationId);
	return Handle ? *Handle : FGameplayAbilitySpecHandle();
}

bool UAbilitySystemComponentBase::GetActionResult(const FGuid& ActivationId, FAbilityActionResult& OutResult) const
{
	if (const FAbilityActionResult* Result = RecentActionResults.Find(ActivationId))
	{
		OutResult = *Result;
		return true;
	}
	return false;
}

void UAbilitySystemComponentBase::PublishActionResult(const FAbilityActionResult& Result)
{
	if (!Result.ActivationId.IsValid() || Result.Outcome == EAbilityActionOutcome::None || RecentActionResults.Contains(Result.ActivationId))
	{
		return;
	}
	PendingActionReplies.Remove(Result.ActivationId);
	RecentActionResults.Add(Result.ActivationId, Result);
	RecentActionOrder.Add(Result.ActivationId);
	while (RecentActionOrder.Num() > 256)
	{
		RecentActionResults.Remove(RecentActionOrder[0]);
		RecentActionOrder.RemoveAt(0, 1, EAllowShrinking::No);
	}
	if (IsOwnerActorAuthoritative() && GetOwner() && GetOwner()->GetNetConnection())
	{
		ClientReceiveActionResult(Result);
	}
	OnActionFinished.Broadcast(Result);
}

void UAbilitySystemComponentBase::ClientReceiveActionResult_Implementation(const FAbilityActionResult& Result)
{
	const FGameplayAbilitySpecHandle* PendingHandle = PendingActionReplies.Find(Result.ActivationId);
	if (!Result.ActivationId.IsValid() || !Result.AbilityHandle.IsValid() || (PendingHandle && *PendingHandle != Result.AbilityHandle) || Result.Outcome == EAbilityActionOutcome::None || RecentActionResults.Contains(Result.ActivationId))
	{
		return;
	}
	DeferredAuthoritativeResults.Add(Result.ActivationId, Result);
	if (UAbilityBase* Action = FindActiveAction(Result.ActivationId))
	{
		Action->ResolveAuthoritativeResult(Result);
	}
	NotifyActionInstanceEnded(Result.ActivationId);
}

void UAbilitySystemComponentBase::NotifyActionInstanceEnded(const FGuid& ActivationId)
{
	if (IsActionActive(ActivationId))
	{
		return;
	}
	FAbilityActionResult Result;
	if (DeferredAuthoritativeResults.RemoveAndCopyValue(ActivationId, Result))
	{
		PublishActionResult(Result);
	}
}

bool UAbilitySystemComponentBase::BindOwnerAvatar(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (bAvatarUnbinding || !IsValid(InOwnerActor) || !IsValid(InAvatarActor) || InOwnerActor != GetOwner() || InOwnerActor->GetWorld() != InAvatarActor->GetWorld())
	{
		return false;
	}
	if (AbilityActorInfo.IsValid() && AbilityActorInfo->OwnerActor == InOwnerActor && AbilityActorInfo->AvatarActor == InAvatarActor)
	{
		RefreshAbilityActorInfo();
		return true;
	}
	if (AbilityActorInfo.IsValid() && AbilityActorInfo->AvatarActor.IsValid())
	{
		UnbindAvatar(AbilityActionTags::AvatarRemoved);
		if (bAvatarUnbinding)
		{
			return false;
		}
	}
	InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	return true;
}

void UAbilitySystemComponentBase::UnbindAvatar(FGameplayTag Reason)
{
	bAvatarUnbinding = true;
	AvatarUnbindReason = Reason.IsValid() ? Reason : AbilityActionTags::AvatarRemoved;
	TArray<TWeakObjectPtr<UAbilityBase>> Instances;
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			if (UAbilityBase* Action = Cast<UAbilityBase>(Instance); Action && Action->IsActive())
			{
				Instances.Add(Action);
			}
		}
	}
	for (const TWeakObjectPtr<UAbilityBase>& Action : Instances)
	{
		if (Action.IsValid() && Action->IsActive())
		{
			Action->FailAction(AvatarUnbindReason);
		}
	}
	TryFinishAvatarUnbind();
}

void UAbilitySystemComponentBase::TryFinishAvatarUnbind()
{
	AvatarUnbindTimer.Invalidate();
	if (!bAvatarUnbinding)
	{
		return;
	}
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.IsActive())
		{
			if (GetWorld() && !GetWorld()->GetTimerManager().IsTimerActive(AvatarUnbindTimer))
			{
				AvatarUnbindTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAbilitySystemComponentBase::TryFinishAvatarUnbind);
			}
			return;
		}
	}
	TArray<FGuid> PendingIds;
	PendingActionReplies.GenerateKeyArray(PendingIds);
	for (const FGuid& Id : PendingIds)
	{
		if (!PendingActionReplies.Contains(Id))
		{
			continue;
		}
		FAbilityActionResult Result;
		Result.ActivationId = Id;
		Result.AbilityHandle = PendingActionReplies.FindChecked(Id);
		Result.Outcome = EAbilityActionOutcome::Cancelled;
		Result.Reason = AvatarUnbindReason;
		PublishActionResult(Result);
	}
	StopAllCurrentMontages(0.f);
	LocalAnimMontageInfoForMeshes.Reset();
	if (IsOwnerActorAuthoritative())
	{
		RepAnimMontageInfoForMeshes.Reset();
	}
	ClearActorInfo();
	bAvatarUnbinding = false;
	DeferredAuthoritativeResults.Reset();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AvatarUnbindTimer);
		GetWorld()->GetTimerManager().ClearTimer(GrantReleaseTimer);
	}
}

void UAbilitySystemComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAvatar(AbilityActionTags::AvatarRemoved);
	Super::EndPlay(EndPlayReason);
	TryFinishAvatarUnbind();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AvatarUnbindTimer);
		GetWorld()->GetTimerManager().ClearTimer(GrantReleaseTimer);
	}
	OnActionFinished.Clear();
}

void UAbilitySystemComponentBase::SetAbilityEntityId(FGuid InEntityId)
{
	AbilityEntityId = InEntityId;
}

FGuid UAbilitySystemComponentBase::GetAbilityEntityId() const
{
	return AbilityEntityId;
}

FGameplayEffectContextHandle UAbilitySystemComponentBase::MakeEffectContext() const
{
	FGameplayEffectContextBase* Context = new FGameplayEffectContextBase();
	Context->SourceId = AbilityEntityId;
	Context->EffectInstanceId = FGuid::NewGuid();
	if (AbilityActorInfo.IsValid())
	{
		Context->AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	}
	return FGameplayEffectContextHandle(Context);
}

FGameplayEffectSpecHandle UAbilitySystemComponentBase::MakeOutgoingSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle Context) const
{
	if (!GameplayEffectClass || !FMath::IsFinite(Level) || Level < 0.f)
	{
		return FGameplayEffectSpecHandle();
	}
	FGameplayEffectContextHandle UniqueContext = Context.IsValid() ? Context.Duplicate() : MakeEffectContext();
	if (UniqueContext.Get()->GetScriptStruct() != FGameplayEffectContextBase::StaticStruct())
	{
		return FGameplayEffectSpecHandle();
	}
	static_cast<FGameplayEffectContextBase*>(UniqueContext.Get())->EffectInstanceId = FGuid::NewGuid();
	static_cast<FGameplayEffectContextBase*>(UniqueContext.Get())->GrantSourceId.Invalidate();
	return Super::MakeOutgoingSpec(GameplayEffectClass, Level, UniqueContext);
}

bool UAbilitySystemComponentBase::CanReplaceGrantSource(const FAbilityGrantSourceState& Source) const
{
	for (FGameplayAbilitySpecHandle Handle : Source.AbilityHandles)
	{
		const FGameplayAbilitySpec* Spec = const_cast<UAbilitySystemComponentBase*>(this)->FindAbilitySpecFromHandle(Handle);
		if (!Spec)
		{
			continue;
		}
		for (UGameplayAbility* Ability : Spec->GetAbilityInstances())
		{
			if (Ability && Ability->IsActive() && !Ability->CanBeCanceled())
			{
				return false;
			}
		}
	}
	return true;
}

namespace
{
	bool IsExternallyPersistedEffect(const FActiveGameplayEffect* Active)
	{
		const FGameplayEffectContext* Context = Active ? Active->Spec.GetContext().Get() : nullptr;
		return Context && Context->GetScriptStruct() == FGameplayEffectContextBase::StaticStruct()
			&& !static_cast<const FGameplayEffectContextBase*>(Context)->ExternalPersistenceDomain.IsNone();
	}

	bool IsValidInventoryGrantSource(UObject* InObject, const AActor* InAvatar)
	{
		const UInventoryAbilitySource* Source = Cast<UInventoryAbilitySource>(InObject);
		FAbilityItem Item;
		return Source && InAvatar && Source->Avatar.Get() == InAvatar && Source->ResolveItem(Item);
	}
}

bool UAbilitySystemComponentBase::SetGrantSource(FGuid SourceId, const TArray<FAbilityGrantDefinition>& Abilities, const TArray<TSubclassOf<UEffectBase>>& Effects, int32 Level)
{
	if (!IsOwnerActorAuthoritative() || !SourceId.IsValid() || Level < 1 || Level > 100000 || Abilities.Num() > 128 || Effects.Num() > 128 || bGrantMutation || RevokedGrantSources.Contains(SourceId))
	{
		return false;
	}
	TSet<FPrimaryAssetId> DefinitionIds;
	for (const FAbilityGrantDefinition& Definition : Abilities)
	{
		const UAbilityBase* Ability = Definition.AbilityClass ? Definition.AbilityClass->GetDefaultObject<UAbilityBase>() : nullptr;
		if (!Definition.DefinitionId.IsValid() || DefinitionIds.Contains(Definition.DefinitionId) || !Ability || !Ability->ActionTag.IsValid() || Definition.Level < 1 || Definition.Level > 100000 || Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor || (Definition.SourceObject && !Definition.SourceObject->IsAsset() && !Definition.SourceObject->HasAnyFlags(RF_ClassDefaultObject) && !(Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly && IsValidInventoryGrantSource(Definition.SourceObject, GetAvatarActor()))))
		{
			return false;
		}
		DefinitionIds.Add(Definition.DefinitionId);
	}
	for (TSubclassOf<UEffectBase> EffectClass : Effects)
	{
		const UEffectBase* Effect = EffectClass ? EffectClass->GetDefaultObject<UEffectBase>() : nullptr;
		if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite || Effect->Executions.Num() != 0 || Effect->Period.GetValueAtLevel(Level) != 0.f || Effect->StackingType != EGameplayEffectStackingType::None)
		{
			return false;
		}
	}
	if (const FAbilityGrantSourceState* Existing = GrantSources.Find(SourceId))
	{
		FAbilityGrantSourceDefinition Desired;
		Desired.SourceId = SourceId;
		Desired.Abilities = Abilities;
		Desired.Effects = Effects;
		Desired.Level = Level;
		if (MatchesGrantSource(*Existing, Desired))
		{
			return true;
		}
		if (!CanReplaceGrantSource(*Existing))
		{
			return false;
		}
	}
	TArray<FGameplayAbilitySpecHandle> GrantedHandles;
	{
	TGuardValue<bool> MutationGuard(bGrantMutation, true);
	FScopedAggregatorOnDirtyBatch AttributeBatch;
	FAbilityGrantSourceState Candidate;
	Candidate.Abilities = Abilities;
	Candidate.Effects = Effects;
	Candidate.Level = Level;
	for (const FAbilityGrantDefinition& Definition : Abilities)
	{
		const FGameplayAbilitySpecHandle Handle = GiveAbility(FGameplayAbilitySpec(Definition.AbilityClass, Definition.Level, INDEX_NONE, Definition.SourceObject));
		if (!Handle.IsValid())
		{
			for (FGameplayAbilitySpecHandle Given : Candidate.AbilityHandles)
			{
				ClearAbility(Given);
			}
			return false;
		}
		Candidate.AbilityHandles.Add(Handle);
	}
	for (TSubclassOf<UEffectBase> EffectClass : Effects)
	{
		FGameplayEffectContextHandle Context = MakeEffectContext();
		Context.AddSourceObject(EffectClass->GetDefaultObject());
		FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, Level, Context);
		if (Spec.IsValid())
		{
			FGameplayEffectContextHandle SourceContext = Spec.Data->GetContext();
			static_cast<FGameplayEffectContextBase*>(SourceContext.Get())->GrantSourceId = SourceId;
		}
		const FActiveGameplayEffectHandle Handle = Spec.IsValid() ? ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
		if (!Handle.IsValid())
		{
			for (FActiveGameplayEffectHandle Applied : Candidate.EffectHandles)
			{
				RemoveActiveGameplayEffect(Applied);
			}
			for (FGameplayAbilitySpecHandle Given : Candidate.AbilityHandles)
			{
				ClearAbility(Given);
			}
			return false;
		}
		Candidate.EffectHandles.Add(Handle);
	}
	RemoveGrantSourceInternal(SourceId);
	GrantSources.Add(SourceId, MoveTemp(Candidate));
	const FAbilityGrantSourceState& Stored = GrantSources.FindChecked(SourceId);
	for (int32 Index = 0; Index < Stored.Abilities.Num(); ++Index)
	{
		FAbilityGrantedEntry Entry;
		Entry.SourceId = SourceId;
		Entry.DefinitionId = Stored.Abilities[Index].DefinitionId;
		Entry.ActionTag = Stored.Abilities[Index].AbilityClass->GetDefaultObject<UAbilityBase>()->ActionTag;
		Entry.AbilityHandle = Stored.AbilityHandles[Index];
		Entry.Level = Stored.Abilities[Index].Level;
		Entry.bActionEnabled = Stored.Abilities[Index].bActionEnabled;
		GrantedEntries.Add(Entry);
	}
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	GrantedHandles = Stored.AbilityHandles;
	}
	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		if (Abilities[Index].bActivateOnGrant)
		{
			FAbilityActionRequest Request;
			Request.AbilityHandle = GrantedHandles[Index];
			Request.DefinitionId = Abilities[Index].DefinitionId;
			Request.ActionTag = Abilities[Index].AbilityClass->GetDefaultObject<UAbilityBase>()->ActionTag;
			Request.TargetActor = GetAvatarActor();
			Request.TargetLocation = GetAvatarActor() ? GetAvatarActor()->GetActorLocation() : FVector::ZeroVector;
			RequestAction(Request);
		}
	}
	return true;
}

void UAbilitySystemComponentBase::RemoveGrantSourceInternal(FGuid SourceId)
{
	SuspendedGrantSources.Remove(SourceId);
	FAbilityGrantSourceState Removed;
	if (!GrantSources.RemoveAndCopyValue(SourceId, Removed))
	{
		return;
	}
	GrantedEntries.RemoveAll([SourceId](const FAbilityGrantedEntry& Entry)
	{
		return Entry.SourceId == SourceId;
	});
	for (FGameplayAbilitySpecHandle Handle : Removed.AbilityHandles)
	{
		if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
		{
			const TArray<UGameplayAbility*> Instances = Spec->GetAbilityInstances();
			for (UGameplayAbility* Instance : Instances)
			{
				if (UAbilityBase* Action = Cast<UAbilityBase>(Instance); Action && Action->IsActive())
				{
					Action->RequestActionCancellation(AbilityActionTags::SourceRemoved);
				}
			}
		}
		ClearAbility(Handle);
	}
	for (FActiveGameplayEffectHandle Handle : Removed.EffectHandles)
	{
		RemoveActiveGameplayEffect(Handle);
	}
}

bool UAbilitySystemComponentBase::RemoveGrantSource(FGuid SourceId)
{
	if (!IsOwnerActorAuthoritative() || bGrantMutation)
	{
		return false;
	}
	const FAbilityGrantSourceState* Source = GrantSources.Find(SourceId);
	if (!Source)
	{
		return true;
	}
	if (!CanReplaceGrantSource(*Source))
	{
		return false;
	}
	TGuardValue<bool> MutationGuard(bGrantMutation, true);
	RemoveGrantSourceInternal(SourceId);
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

bool UAbilitySystemComponentBase::FindGrantedAbility(FPrimaryAssetId DefinitionId, FGameplayTag ActionTag, FGameplayAbilitySpecHandle& OutHandle) const
{
	const FAbilityGrantedEntry* Selected = nullptr;
	for (const FAbilityGrantedEntry& Entry : GrantedEntries)
	{
		if (Entry.bActionEnabled && !SuspendedGrantSources.Contains(Entry.SourceId) && (!DefinitionId.IsValid() || Entry.DefinitionId == DefinitionId) && (!ActionTag.IsValid() || Entry.ActionTag == ActionTag))
		{
			const FGameplayAbilitySpec* Spec = const_cast<UAbilitySystemComponentBase*>(this)->FindAbilitySpecFromHandle(Entry.AbilityHandle);
			if (Spec && (!Selected || Entry.Level > Selected->Level || (Entry.Level == Selected->Level && Entry.SourceId.ToString() < Selected->SourceId.ToString())))
			{
				Selected = &Entry;
			}
		}
	}
	OutHandle = Selected ? Selected->AbilityHandle : FGameplayAbilitySpecHandle();
	return Selected != nullptr;
}

const TArray<FAbilityGrantedEntry>& UAbilitySystemComponentBase::GetGrantedEntries() const
{
	return GrantedEntries;
}

bool UAbilitySystemComponentBase::IsGrantEffect(FActiveGameplayEffectHandle Handle) const
{
	if (const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Handle))
	{
		const FGameplayEffectContext* Context = Active->Spec.GetContext().Get();
		if (Context && Context->GetScriptStruct() == FGameplayEffectContextBase::StaticStruct() && static_cast<const FGameplayEffectContextBase*>(Context)->GrantSourceId.IsValid())
		{
			return true;
		}
	}
	for (const TPair<FGuid, FAbilityGrantSourceState>& Source : GrantSources)
	{
		if (Source.Value.EffectHandles.Contains(Handle))
		{
			return true;
		}
	}
	return false;
}

bool UAbilitySystemComponentBase::MatchesGrantSource(const FAbilityGrantSourceState& Existing, const FAbilityGrantSourceDefinition& Desired) const
{
	if (Existing.Level != Desired.Level || Existing.Effects != Desired.Effects || Existing.Abilities.Num() != Desired.Abilities.Num()
		|| Existing.AbilityHandles.Num() != Desired.Abilities.Num() || Existing.EffectHandles.Num() != Desired.Effects.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Desired.Abilities.Num(); ++Index)
	{
		const FAbilityGrantDefinition& Left = Existing.Abilities[Index];
		const FAbilityGrantDefinition& Right = Desired.Abilities[Index];
		const FGameplayAbilitySpec* Spec = const_cast<UAbilitySystemComponentBase*>(this)->FindAbilitySpecFromHandle(Existing.AbilityHandles[Index]);
		if (Left.DefinitionId != Right.DefinitionId || Left.AbilityClass != Right.AbilityClass || Left.SourceObject != Right.SourceObject
			|| Left.Level != Right.Level || Left.bActivateOnGrant != Right.bActivateOnGrant || Left.bActionEnabled != Right.bActionEnabled || !Spec || !Spec->Ability
			|| Spec->Ability->GetClass() != Right.AbilityClass || Spec->SourceObject.Get() != Right.SourceObject || Spec->Level != Right.Level)
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Desired.Effects.Num(); ++Index)
	{
		const FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Existing.EffectHandles[Index]);
		if (!Effect || Effect->Spec.Def->GetClass() != Desired.Effects[Index])
		{
			return false;
		}
	}
	return true;
}

bool UAbilitySystemComponentBase::ValidateGrantSourceSet(const TSet<FGuid>& OwnedSources, const TArray<FAbilityGrantSourceDefinition>& DesiredSources) const
{
	if (!IsOwnerActorAuthoritative() || bGrantMutation || OwnedSources.Num() > 1024 || DesiredSources.Num() > 1024)
	{
		return false;
	}
	TSet<FGuid> DesiredIds;
	for (const FAbilityGrantSourceDefinition& Desired : DesiredSources)
	{
		if (!Desired.SourceId.IsValid() || RevokedGrantSources.Contains(Desired.SourceId) || DesiredIds.Contains(Desired.SourceId) || Desired.Level < 1 || Desired.Level > 100000
			|| Desired.Abilities.Num() > 128 || Desired.Effects.Num() > 128
			|| (GrantSources.Contains(Desired.SourceId) && !OwnedSources.Contains(Desired.SourceId)))
		{
			return false;
		}
		DesiredIds.Add(Desired.SourceId);
		TSet<FPrimaryAssetId> DefinitionIds;
		for (const FAbilityGrantDefinition& Definition : Desired.Abilities)
		{
			const UAbilityBase* Ability = Definition.AbilityClass ? Definition.AbilityClass->GetDefaultObject<UAbilityBase>() : nullptr;
			if (!Definition.DefinitionId.IsValid() || DefinitionIds.Contains(Definition.DefinitionId) || !Ability || !Ability->ActionTag.IsValid()
				|| Definition.Level < 1 || Definition.Level > 100000 || Definition.bActivateOnGrant
				|| Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor
				|| (Definition.SourceObject && !Definition.SourceObject->IsAsset() && !Definition.SourceObject->HasAnyFlags(RF_ClassDefaultObject) && !(Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly && IsValidInventoryGrantSource(Definition.SourceObject, GetAvatarActor()))))
			{
				return false;
			}
			DefinitionIds.Add(Definition.DefinitionId);
		}
		for (TSubclassOf<UEffectBase> EffectClass : Desired.Effects)
		{
			const UEffectBase* Effect = EffectClass ? EffectClass->GetDefaultObject<UEffectBase>() : nullptr;
			if (!Effect || !Effect->SupportsAttributeGrant(Desired.Level))
			{
				return false;
			}
			for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
			{
				if (!HasAttributeSetForAttribute(Modifier.Attribute))
				{
					return false;
				}
			}
		}
	}
	for (const FGuid& SourceId : OwnedSources)
	{
		if (!SourceId.IsValid())
		{
			return false;
		}
		const FAbilityGrantSourceState* Existing = GrantSources.Find(SourceId);
		if (!Existing)
		{
			continue;
		}
		const FAbilityGrantSourceDefinition* Desired = DesiredSources.FindByPredicate([SourceId](const FAbilityGrantSourceDefinition& Entry)
		{
			return Entry.SourceId == SourceId;
		});
		if (Desired && MatchesGrantSource(*Existing, *Desired))
		{
			continue;
		}
		for (FGameplayAbilitySpecHandle Handle : Existing->AbilityHandles)
		{
			const FGameplayAbilitySpec* Spec = const_cast<UAbilitySystemComponentBase*>(this)->FindAbilitySpecFromHandle(Handle);
			if (Spec && Spec->IsActive())
			{
				return false;
			}
		}
	}
	return true;
}

bool UAbilitySystemComponentBase::IsGrantSourceChangePending(const TSet<FGuid>& OwnedSources, const TArray<FAbilityGrantSourceDefinition>& DesiredSources) const
{
    if (bGrantMutation) return true;
    for (const FAbilityGrantSourceDefinition& Desired : DesiredSources)
    {
        if (RevokedGrantSources.Contains(Desired.SourceId)) return true;
    }
    for (const FGuid& SourceId : OwnedSources)
    {
        const FAbilityGrantSourceState* Existing = GrantSources.Find(SourceId);
        if (!Existing) continue;
        const FAbilityGrantSourceDefinition* Desired = DesiredSources.FindByPredicate([SourceId](const FAbilityGrantSourceDefinition& Entry)
        {
            return Entry.SourceId == SourceId;
        });
        if (Desired && MatchesGrantSource(*Existing, *Desired)) continue;
        if (!CanReplaceGrantSource(*Existing)) return true;
    }
    return false;
}

bool UAbilitySystemComponentBase::ReplaceGrantSourceSet(const TSet<FGuid>& OwnedSources, const TArray<FAbilityGrantSourceDefinition>& DesiredSources)
{
	if (!ValidateGrantSourceSet(OwnedSources, DesiredSources))
	{
		return false;
	}
	const TWeakObjectPtr<AActor> TransactionAvatar(GetAvatarActor());
	const FGuid TransactionEntity = AbilityEntityId;
	TGuardValue<bool> MutationGuard(bGrantMutation, true);
	FScopedAggregatorOnDirtyBatch AttributeBatch;
	TMap<FGuid, FAbilityGrantSourceState> Staged;
	TSet<FGuid> DesiredIds;
	auto Rollback = [this, &Staged]()
	{
		for (const TPair<FGuid, FAbilityGrantSourceState>& Pair : Staged)
		{
			for (FActiveGameplayEffectHandle Handle : Pair.Value.EffectHandles)
			{
				RemoveActiveGameplayEffect(Handle);
			}
			for (FGameplayAbilitySpecHandle Handle : Pair.Value.AbilityHandles)
			{
				ClearAbility(Handle);
			}
		}
	};
	for (const FAbilityGrantSourceDefinition& Desired : DesiredSources)
	{
		DesiredIds.Add(Desired.SourceId);
		const FAbilityGrantSourceState* Existing = GrantSources.Find(Desired.SourceId);
		if (Existing && MatchesGrantSource(*Existing, Desired))
		{
			continue;
		}
		FAbilityGrantSourceState& Candidate = Staged.Add(Desired.SourceId);
		Candidate.Abilities = Desired.Abilities;
		Candidate.Effects = Desired.Effects;
		Candidate.Level = Desired.Level;
		for (const FAbilityGrantDefinition& Definition : Desired.Abilities)
		{
			const FGameplayAbilitySpecHandle Handle = GiveAbility(FGameplayAbilitySpec(Definition.AbilityClass, Definition.Level, INDEX_NONE, Definition.SourceObject));
			if (!Handle.IsValid() || !FindAbilitySpecFromHandle(Handle))
			{
				if (Handle.IsValid())
				{
					ClearAbility(Handle);
				}
				Rollback();
				return false;
			}
			Candidate.AbilityHandles.Add(Handle);
		}
		for (TSubclassOf<UEffectBase> EffectClass : Desired.Effects)
		{
			FGameplayEffectContextHandle Context = MakeEffectContext();
			Context.AddSourceObject(EffectClass->GetDefaultObject());
			FGameplayEffectSpecHandle Spec = MakeOutgoingSpec(EffectClass, Desired.Level, Context);
			if (!Spec.IsValid())
			{
				Rollback();
				return false;
			}
			static_cast<FGameplayEffectContextBase*>(Spec.Data->GetContext().Get())->GrantSourceId = Desired.SourceId;
			const FActiveGameplayEffectHandle Handle = ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			if (!Handle.IsValid() || !GetActiveGameplayEffect(Handle))
			{
				if (Handle.IsValid())
				{
					RemoveActiveGameplayEffect(Handle);
				}
				Rollback();
				return false;
			}
			Candidate.EffectHandles.Add(Handle);
		}
	}
	if (TransactionAvatar.Get() != GetAvatarActor() || TransactionEntity != AbilityEntityId)
	{
		Rollback();
		return false;
	}
	for (const TPair<FGuid, FAbilityGrantSourceState>& Pair : Staged)
	{
		const FAbilityGrantSourceDefinition* Desired = DesiredSources.FindByPredicate([&Pair](const FAbilityGrantSourceDefinition& Entry)
		{
			return Entry.SourceId == Pair.Key;
		});
		if (!Desired || !MatchesGrantSource(Pair.Value, *Desired))
		{
			Rollback();
			return false;
		}
	}
	for (const FGuid& SourceId : OwnedSources)
	{
		if (!DesiredIds.Contains(SourceId) || Staged.Contains(SourceId))
		{
			RemoveGrantSourceInternal(SourceId);
		}
	}
	for (TPair<FGuid, FAbilityGrantSourceState>& Pair : Staged)
	{
		const FGuid SourceId = Pair.Key;
		GrantSources.Add(SourceId, MoveTemp(Pair.Value));
		const FAbilityGrantSourceState& Stored = GrantSources.FindChecked(SourceId);
		for (int32 Index = 0; Index < Stored.Abilities.Num(); ++Index)
		{
			FAbilityGrantedEntry Entry;
			Entry.SourceId = SourceId;
			Entry.DefinitionId = Stored.Abilities[Index].DefinitionId;
			Entry.ActionTag = Stored.Abilities[Index].AbilityClass->GetDefaultObject<UAbilityBase>()->ActionTag;
			Entry.AbilityHandle = Stored.AbilityHandles[Index];
			Entry.Level = Stored.Abilities[Index].Level;
			Entry.bActionEnabled = Stored.Abilities[Index].bActionEnabled;
			GrantedEntries.Add(Entry);
		}
	}
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

bool UAbilitySystemComponentBase::CapturePersistentEffects(TArray<FAbilityEffectSaveData>& OutEffects) const
{
	OutEffects.Reset();
	if (!IsOwnerActorAuthoritative() || !GetWorld())
	{
		return false;
	}
	TArray<FAbilityEffectSaveData> Captured;
	TSet<FGuid> InstanceIds;
	for (FActiveGameplayEffectHandle Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Handle);
		const UEffectBase* Definition = Active ? Cast<UEffectBase>(Active->Spec.Def) : nullptr;
		if (!Definition || !Definition->bPersistAcrossSave || IsGrantEffect(Handle))
		{
			continue;
		}
		const FGameplayEffectContext* Context = Active->Spec.GetContext().Get();
		if (Context && Context->GetScriptStruct() == FGameplayEffectContextBase::StaticStruct()
			&& !static_cast<const FGameplayEffectContextBase*>(Context)->ExternalPersistenceDomain.IsNone())
		{
			continue;
		}
		if (!Definition->SupportsPersistence(Active->Spec.GetLevel()) || !Context || Context->GetScriptStruct() != FGameplayEffectContextBase::StaticStruct())
		{
			return false;
		}
		const FGameplayEffectContextBase* SourceContext = static_cast<const FGameplayEffectContextBase*>(Context);
		if (!SourceContext->ExternalPersistenceDomain.IsNone())
		{
			continue;
		}
		if (!SourceContext->SourceId.IsValid() || !SourceContext->EffectInstanceId.IsValid() || InstanceIds.Contains(SourceContext->EffectInstanceId))
		{
			return false;
		}
		InstanceIds.Add(SourceContext->EffectInstanceId);
		FAbilityEffectSaveData Record;
		Record.EffectClass = FSoftClassPath(Definition->GetClass());
		Record.SourceId = SourceContext->SourceId;
		Record.EffectInstanceId = SourceContext->EffectInstanceId;
		if (const UObject* SourceObject = Context->GetSourceObject())
		{
			if (!SourceObject->IsAsset() && !SourceObject->HasAnyFlags(RF_ClassDefaultObject))
			{
				return false;
			}
			Record.SourceObject = FSoftObjectPath(SourceObject);
		}
		Record.Level = Active->Spec.GetLevel();
		Record.StackCount = Active->Spec.GetStackCount();
		Record.RemainingDuration = Definition->DurationPolicy == EGameplayEffectDurationType::Infinite ? -1.f : FMath::Max(0.f, Active->GetTimeRemaining(GetWorld()->GetTimeSeconds()));
		Record.SavedUtcSeconds = static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
		Record.SetByCallerMagnitudes = Active->Spec.SetByCallerTagMagnitudes;
		Record.SetByCallerNameMagnitudes = Active->Spec.SetByCallerNameMagnitudes;
		Record.CapturedSourceTags = *Active->Spec.CapturedSourceTags.GetAggregatedTags();
		Record.DynamicGrantedTags = Active->Spec.DynamicGrantedTags;
		Record.DynamicAssetTags = Active->Spec.GetDynamicAssetTags();
		if (Captured.Num() >= 4096 || !FMath::IsFinite(Record.Level) || Record.Level < 1.f || Record.Level > 100000.f || Record.StackCount < 1 || Record.StackCount > 100000 || !FMath::IsFinite(Record.RemainingDuration))
		{
			return false;
		}
		for (const TPair<FGameplayTag, float>& Magnitude : Record.SetByCallerMagnitudes)
		{
			if (!Magnitude.Key.IsValid() || !FMath::IsFinite(Magnitude.Value))
			{
				return false;
			}
		}
		for (const TPair<FName, float>& Magnitude : Record.SetByCallerNameMagnitudes)
		{
			if (Magnitude.Key.IsNone() || !FMath::IsFinite(Magnitude.Value))
			{
				return false;
			}
		}
		Captured.Add(MoveTemp(Record));
	}
	OutEffects = MoveTemp(Captured);
	return true;
}

void UAbilitySystemComponentBase::GatherPersistentEffectAssets(const TArray<FAbilityEffectSaveData>& Effects, TArray<FSoftObjectPath>& OutAssets)
{
	TSet<FSoftObjectPath> Unique;
	for (const FSoftObjectPath& Existing : OutAssets)
	{
		if (!Existing.IsNull())
		{
			Unique.Add(Existing);
		}
	}
	for (const FAbilityEffectSaveData& Record : Effects)
	{
		if (!Record.EffectClass.IsNull())
		{
			Unique.Add(Record.EffectClass);
		}
		if (!Record.SourceObject.IsNull())
		{
			Unique.Add(Record.SourceObject);
		}
	}
	OutAssets = Unique.Array();
	OutAssets.Sort([](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
	{
		return Left.ToString() < Right.ToString();
	});
}

bool UAbilitySystemComponentBase::ValidatePersistentEffects(const TArray<FAbilityEffectSaveData>& Effects) const
{
	if (!IsOwnerActorAuthoritative() || !GetWorld() || Effects.Num() > 4096)
	{
		return false;
	}
	TSet<FGuid> RequestedIds;
	for (const FAbilityEffectSaveData& Record : Effects)
	{
		UClass* LoadedClass = Record.EffectClass.ResolveClass();
		const UEffectBase* Definition = LoadedClass && LoadedClass->IsChildOf(UEffectBase::StaticClass()) ? LoadedClass->GetDefaultObject<UEffectBase>() : nullptr;
		if (!Definition || !Definition->bPersistAcrossSave || !Definition->SupportsPersistence(Record.Level) || !Record.SourceId.IsValid() || !Record.EffectInstanceId.IsValid() || RequestedIds.Contains(Record.EffectInstanceId) || !FMath::IsFinite(Record.Level) || Record.Level < 1.f || Record.Level > 100000.f || Record.StackCount < 1 || Record.StackCount > 100000 || !FMath::IsFinite(Record.RemainingDuration) || !FMath::IsFinite(Record.SavedUtcSeconds) || (!Record.SourceObject.IsNull() && (!Record.SourceObject.ResolveObject() || (!Record.SourceObject.ResolveObject()->IsAsset() && !Record.SourceObject.ResolveObject()->HasAnyFlags(RF_ClassDefaultObject)))))
		{
			return false;
		}
		if ((Definition->DurationPolicy == EGameplayEffectDurationType::Infinite && Record.RemainingDuration != -1.f) || (Definition->DurationPolicy != EGameplayEffectDurationType::Infinite && Record.RemainingDuration < 0.f) || (Definition->StackingType == EGameplayEffectStackingType::None && Record.StackCount != 1) || (Definition->StackLimitCount > 0 && Record.StackCount > Definition->StackLimitCount))
		{
			return false;
		}
		for (const TPair<FGameplayTag, float>& Magnitude : Record.SetByCallerMagnitudes)
		{
			if (!Magnitude.Key.IsValid() || !FMath::IsFinite(Magnitude.Value))
			{
				return false;
			}
		}
		for (const TPair<FName, float>& Magnitude : Record.SetByCallerNameMagnitudes)
		{
			if (Magnitude.Key.IsNone() || !FMath::IsFinite(Magnitude.Value))
			{
				return false;
			}
		}
		RequestedIds.Add(Record.EffectInstanceId);
	}
	return true;
}

bool UAbilitySystemComponentBase::RestorePersistentEffects(const TArray<FAbilityEffectSaveData>& Effects)
{
	if (!ValidatePersistentEffects(Effects))
	{
		return false;
	}
	TSet<FGuid> RequestedIds;
	for (const FAbilityEffectSaveData& Record : Effects)
	{
		RequestedIds.Add(Record.EffectInstanceId);
	}
	TSet<FGuid> ExistingIds;
	TArray<TSubclassOf<UEffectBase>> ExistingAggregates;
	for (FActiveGameplayEffectHandle Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Handle);
		const UEffectBase* Definition = Active ? Cast<UEffectBase>(Active->Spec.Def) : nullptr;
		if (!Definition || !Definition->bPersistAcrossSave || IsGrantEffect(Handle))
		{
			continue;
		}
		const FGameplayEffectContext* Context = Active->Spec.GetContext().Get();
		if (!Context || Context->GetScriptStruct() != FGameplayEffectContextBase::StaticStruct())
		{
			return false;
		}
		const FGuid Id = static_cast<const FGameplayEffectContextBase*>(Context)->EffectInstanceId;
		if (!RequestedIds.Contains(Id))
		{
			return false;
		}
		ExistingIds.Add(Id);
		if (Definition->StackingType != EGameplayEffectStackingType::None)
		{
			ExistingAggregates.Add(Definition->GetClass());
		}
	}
	TArray<FGameplayEffectSpecHandle> PendingSpecs;
	TSet<UClass*> PendingAggregateClasses;
	const double CurrentUtc = static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
	for (const FAbilityEffectSaveData& Record : Effects)
	{
		if (ExistingIds.Contains(Record.EffectInstanceId))
		{
			continue;
		}
		UClass* LoadedClass = Record.EffectClass.ResolveClass();
		const UEffectBase* Definition = LoadedClass->GetDefaultObject<UEffectBase>();
		float Remaining = Record.RemainingDuration;
		if (Remaining >= 0.f && Definition->bAdvanceWhileOffline)
		{
			Remaining = FMath::Max(0.f, Remaining - static_cast<float>(FMath::Max(0.0, CurrentUtc - Record.SavedUtcSeconds)));
		}
		if (Remaining == 0.f)
		{
			continue;
		}
		if (Definition->StackingType != EGameplayEffectStackingType::None)
		{
			if (ExistingAggregates.Contains(LoadedClass) || PendingAggregateClasses.Contains(LoadedClass))
			{
				return false;
			}
			PendingAggregateClasses.Add(LoadedClass);
		}
		FGameplayEffectContextBase* Context = new FGameplayEffectContextBase();
		Context->SourceId = Record.SourceId;
		Context->EffectInstanceId = Record.EffectInstanceId;
		Context->AddSourceObject(Record.SourceObject.ResolveObject());
		FGameplayEffectSpecHandle Spec(new FGameplayEffectSpec(Definition, FGameplayEffectContextHandle(Context), Record.Level));
		Spec.Data->SetByCallerTagMagnitudes = Record.SetByCallerMagnitudes;
		Spec.Data->SetByCallerNameMagnitudes = Record.SetByCallerNameMagnitudes;
		Spec.Data->CapturedSourceTags.GetActorTags() = Record.CapturedSourceTags;
		Spec.Data->DynamicGrantedTags = Record.DynamicGrantedTags;
		Spec.Data->AppendDynamicAssetTags(Record.DynamicAssetTags);
		Spec.Data->SetStackCount(Record.StackCount);
		Spec.Data->SetDuration(Remaining, true);
		PendingSpecs.Add(MoveTemp(Spec));
	}
	TArray<FActiveGameplayEffectHandle> Applied;
	for (const FGameplayEffectSpecHandle& Spec : PendingSpecs)
	{
		const FActiveGameplayEffectHandle Handle = ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (!Handle.IsValid())
		{
			for (FActiveGameplayEffectHandle RollbackHandle : Applied)
			{
				RemoveActiveGameplayEffect(RollbackHandle);
			}
			return false;
		}
		Applied.Add(Handle);
	}
	return true;
}

void UAbilitySystemComponentBase::RemoveEffectsOnDeath()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}
	for (FActiveGameplayEffectHandle Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Handle);
		const UEffectBase* Definition = Active ? Cast<UEffectBase>(Active->Spec.Def) : nullptr;
		if (Definition && Definition->bRemoveOnDeath && !IsGrantEffect(Handle) && !IsExternallyPersistedEffect(Active))
		{
			RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UAbilitySystemComponentBase::ClearPersistentEffects()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}
	for (FActiveGameplayEffectHandle Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Active = GetActiveGameplayEffect(Handle);
		const UEffectBase* Definition = Active ? Cast<UEffectBase>(Active->Spec.Def) : nullptr;
		if (Definition && Definition->bPersistAcrossSave && !IsGrantEffect(Handle) && !IsExternallyPersistedEffect(Active))
		{
			RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UAbilitySystemComponentBase::ClearIndependentEffects()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}
	for (FActiveGameplayEffectHandle Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		if (!IsGrantEffect(Handle))
		{
			RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UAbilitySystemComponentBase::SetGrantSourceSuspended(FGuid SourceId, bool bSuspended)
{
	if (!IsOwnerActorAuthoritative() || !SourceId.IsValid()) return;
	if (bSuspended) SuspendedGrantSources.Add(SourceId);
	else if (!RevokedGrantSources.Contains(SourceId)) SuspendedGrantSources.Remove(SourceId);
}

void UAbilitySystemComponentBase::RevokeGrantSource(FGuid SourceId)
{
	if (!IsOwnerActorAuthoritative() || !SourceId.IsValid()) return;
	SuspendedGrantSources.Add(SourceId);
	RevokedGrantSources.Add(SourceId);
	TryReleaseRevokedGrantSources();
}

void UAbilitySystemComponentBase::TryReleaseRevokedGrantSources()
{
	if (!IsOwnerActorAuthoritative() || bReleasingGrantSources) return;
	TGuardValue<bool> ReleaseGuard(bReleasingGrantSources, true);
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(GrantReleaseTimer);
	if (!bGrantMutation)
	{
		const TArray<FGuid> Pending = RevokedGrantSources.Array();
		for (const FGuid& SourceId : Pending)
		{
			const FAbilityGrantSourceState* Source = GrantSources.Find(SourceId);
			TArray<TWeakObjectPtr<UAbilityBase>> Actions;
			if (Source)
			{
				for (FGameplayAbilitySpecHandle Handle : Source->AbilityHandles)
				{
					if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
					{
						for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
						{
							if (UAbilityBase* Action = Cast<UAbilityBase>(Instance); Action && Action->IsActive()) Actions.Add(Action);
						}
					}
				}
			}
			for (const TWeakObjectPtr<UAbilityBase>& Action : Actions)
			{
				if (Action.IsValid() && Action->IsActive()) Action->FailAction(AbilityActionTags::SourceRemoved);
			}
			bool bActive = false;
			Source = GrantSources.Find(SourceId);
			if (Source)
			{
				for (FGameplayAbilitySpecHandle Handle : Source->AbilityHandles)
				{
					const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
					bActive |= Spec && Spec->IsActive();
				}
			}
			if (!bActive && !bGrantMutation && RemoveGrantSource(SourceId))
			{
				RevokedGrantSources.Remove(SourceId);
				SuspendedGrantSources.Remove(SourceId);
			}
		}
	}
	if (GetWorld())
	{
		if (RevokedGrantSources.IsEmpty()) GetWorld()->GetTimerManager().ClearTimer(GrantReleaseTimer);
		else if (!GetWorld()->GetTimerManager().IsTimerActive(GrantReleaseTimer))
			GetWorld()->GetTimerManager().SetTimer(GrantReleaseTimer, this, &ThisClass::TryReleaseRevokedGrantSources, 0.05f, false);
	}
}
