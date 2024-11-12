#include "Ability/Abilities/AbilityBase.h"

#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Common/CommonTypes.h"

UAbilityBase::UAbilityBase()
{
	// Default to Instance Per Actor
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	bActivateAbilityOnGranted = false;
	bActivateOnInput = true;
	bSourceObjectMustEqualCurrentWeaponToActivate = false;
	bCannotActivateWhileInteracting = true;

	// UAbilitySystemGlobals hasn't initialized tags yet to set ActivationBlockedTags
	//ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Vitality.Dead"));

	EffectContainerMap = TMap<FGameplayTag, FGameplayEffectContainer>();
}

void UAbilityBase::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
}

void UAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAbilityBase::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
}

FGameplayAbilityTargetDataHandle UAbilityBase::MakeGameplayAbilityTargetDataHandleFromActorArray(const TArray<AActor*> TargetActors)
{
	if (TargetActors.Num() > 0)
	{
		FGameplayAbilityTargetData_ActorArray* NewData = new FGameplayAbilityTargetData_ActorArray();
		NewData->TargetActorArray.Append(TargetActors);
		return FGameplayAbilityTargetDataHandle(NewData);
	}

	return FGameplayAbilityTargetDataHandle();
}

FGameplayAbilityTargetDataHandle UAbilityBase::MakeGameplayAbilityTargetDataHandleFromHitResults(const TArray<FHitResult> HitResults)
{
	FGameplayAbilityTargetDataHandle TargetData;

	for (const FHitResult& HitResult : HitResults)
	{
		FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);
		TargetData.Add(NewData);
	}

	return TargetData;
}

FGameplayEffectContainerSpec UAbilityBase::MakeEffectContainerSpecFromContainer(const FGameplayEffectContainer& Container, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	// First figure out our actor info
	FGameplayEffectContainerSpec ReturnSpec;
	AActor* OwningActor = GetOwningActorFromActorInfo();
	UAbilitySystemComponentBase* OwningASC = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(OwningActor);

	if (OwningASC)
	{
		// If we have a target type, run the targeting logic. This is optional, targets can be added later
		if (Container.TargetType.Get())
		{
			TArray<FHitResult> HitResults;
			TArray<AActor*> TargetActors;
			const UTargetType* TargetType = Container.TargetType.GetDefaultObject();
			AActor* AvatarActor = GetAvatarActorFromActorInfo();
			TargetType->GetTargets(OwningActor, AvatarActor, EventData, HitResults, TargetActors);
			ReturnSpec.AddTargets(HitResults, TargetActors);
		}
		// If we don't have an override level, use the ability level
		if (OverrideGameplayLevel == INDEX_NONE)
		{
			//OverrideGameplayLevel = OwningASC->GetDefaultAbilityLevel();
			OverrideGameplayLevel = GetAbilityLevel();
		}
		// Build GameplayEffectSpecs for each applied effect
		for (const TSubclassOf<UEffectBase>& EffectClass : Container.TargetGameplayEffectClasses)
		{
			ReturnSpec.TargetGameplayEffectSpecs.Add(MakeOutgoingGameplayEffectSpec(EffectClass, OverrideGameplayLevel));
		}
	}
	return ReturnSpec;
}

FGameplayEffectContainerSpec UAbilityBase::MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	FGameplayEffectContainer* FoundContainer = EffectContainerMap.Find(ContainerTag);

	if (FoundContainer)
	{
		return MakeEffectContainerSpecFromContainer(*FoundContainer, EventData, OverrideGameplayLevel);
	}
	return FGameplayEffectContainerSpec();
}

TArray<FActiveGameplayEffectHandle> UAbilityBase::ApplyEffectContainerSpec(const FGameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;

	// Iterate list of effect specs and apply them to their target data
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		AllEffects.Append(K2_ApplyGameplayEffectSpecToTarget(SpecHandle, ContainerSpec.TargetData));
	}
	return AllEffects;
}

TArray<FActiveGameplayEffectHandle> UAbilityBase::ApplyEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	FGameplayEffectContainerSpec Spec = MakeEffectContainerSpec(ContainerTag, EventData, OverrideGameplayLevel);
	return ApplyEffectContainerSpec(Spec);
}

UObject* UAbilityBase::K2_GetSourceObject(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	return GetSourceObject(Handle, &ActorInfo);
}

bool UAbilityBase::BatchRPCTryActivateAbility(FGameplayAbilitySpecHandle InAbilityHandle, bool EndAbilityImmediately)
{
	UAbilitySystemComponentBase* ASC = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponentFromActorInfo());
	if (ASC)
	{
		return ASC->BatchRPCTryActivateAbility(InAbilityHandle, EndAbilityImmediately);
	}

	return false;
}

void UAbilityBase::ExternalEndAbility()
{
	check(CurrentActorInfo);

	bool bReplicateEndAbility = true;
	bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FString UAbilityBase::GetCurrentPredictionKeyStatus()
{
	UAbilitySystemComponentBase* ASC = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponentFromActorInfo());
	return ASC->ScopedPredictionKey.ToString() + " is valid for more prediction: " + (ASC->ScopedPredictionKey.IsValidForMorePrediction() ? TEXT("true") : TEXT("false"));
}

bool UAbilityBase::IsPredictionKeyValidForMorePrediction() const
{
	UAbilitySystemComponentBase* ASC = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponentFromActorInfo());
	return ASC->ScopedPredictionKey.IsValidForMorePrediction();
}

bool UAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool UAbilityBase::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags) && CheckCost(Handle, *ActorInfo);
}

bool UAbilityBase::CheckCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo) const
{
	return true;
}

void UAbilityBase::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	ApplyCost(Handle, *ActorInfo, ActivationInfo);
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

// void UAbilityBase::SetHUDReticle(TSubclassOf<UHUDReticle> ReticleClass)
// {
// 	APlayerController* PC = Cast<APlayerController>(CurrentActorInfo->PlayerController);
// 	if (PC)
// 	{
// 		PC->SetHUDReticle(ReticleClass);
// 	}
// }
//
// void UAbilityBase::ResetHUDReticle()
// {
// 	APlayerController* PC = Cast<APlayerController>(CurrentActorInfo->PlayerController);
// 	if (PC)
// 	{
// 		AHeroCharacter* Hero = Cast<AHeroCharacter>(CurrentActorInfo->AvatarActor);
// 		if (Hero && Hero->GetCurrentWeapon())
// 		{
// 			PC->SetHUDReticle(Hero->GetCurrentWeapon()->GetPrimaryHUDReticleClass());
// 		}
// 		else
// 		{
// 			PC->SetHUDReticle(nullptr);
// 		}
// 	}
// }

void UAbilityBase::GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	Super::GetCooldownTimeRemainingAndDuration(Handle, ActorInfo, TimeRemaining, CooldownDuration);
}

void UAbilityBase::GetCooldownTimeRemainingAndDuration(float& TimeRemaining, float& CooldownDuration) const
{
	Super::GetCooldownTimeRemainingAndDuration(CurrentSpecHandle, CurrentActorInfo, TimeRemaining, CooldownDuration);
}

void UAbilityBase::SendTargetDataToServer(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (IsPredictingClient())
	{
		UAbilitySystemComponentBase* ASC = Cast<UAbilitySystemComponentBase>(CurrentActorInfo->AbilitySystemComponent.Get());
		check(ASC);

		FScopedPredictionWindow	ScopedPrediction(ASC, IsPredictingClient());

		FGameplayTag ApplicationTag; // Fixme: where would this be useful?
		CurrentActorInfo->AbilitySystemComponent->CallServerSetReplicatedTargetData(CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(), TargetData, ApplicationTag, ASC->ScopedPredictionKey);
	}
}

bool UAbilityBase::IsInputPressed() const
{
	FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	return Spec && Spec->InputPressed;
}

UAnimMontage* UAbilityBase::GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh)
{
	FAbilityMeshMontage AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		return AbilityMeshMontage.Montage;
	}

	return nullptr;
}

void UAbilityBase::SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());

	FAbilityMeshMontage AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		AbilityMeshMontage.Montage = InCurrentMontage;
	}
	else
	{
		CurrentAbilityMeshMontages.Add(FAbilityMeshMontage(InMesh, InCurrentMontage));
	}
}

bool UAbilityBase::FindAbillityMeshMontage(USkeletalMeshComponent* InMesh, FAbilityMeshMontage& InAbilityMeshMontage)
{
	for (FAbilityMeshMontage& MeshMontage : CurrentAbilityMeshMontages)
	{
		if (MeshMontage.Mesh == InMesh)
		{
			InAbilityMeshMontage = MeshMontage;
			return true;
		}
	}

	return false;
}

void UAbilityBase::MontageJumpToSectionForMesh(USkeletalMeshComponent* InMesh, FName SectionName)
{
	check(CurrentActorInfo);

	UAbilitySystemComponentBase* const AbilitySystemComponent = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponentFromActorInfo_Checked());
	if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
	{
		AbilitySystemComponent->CurrentMontageJumpToSectionForMesh(InMesh, SectionName);
	}
}

void UAbilityBase::MontageSetNextSectionNameForMesh(USkeletalMeshComponent* InMesh, FName FromSectionName, FName ToSectionName)
{
	check(CurrentActorInfo);

	UAbilitySystemComponentBase* const AbilitySystemComponent = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponentFromActorInfo_Checked());
	if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
	{
		AbilitySystemComponent->CurrentMontageSetNextSectionNameForMesh(InMesh, FromSectionName, ToSectionName);
	}
}

void UAbilityBase::MontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UAbilitySystemComponentBase* const AbilitySystemComponent = Cast<UAbilitySystemComponentBase>(CurrentActorInfo->AbilitySystemComponent.Get());
	if (AbilitySystemComponent != nullptr)
	{
		// We should only stop the current montage if we are the animating ability
		if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
		{
			AbilitySystemComponent->CurrentMontageStopForMesh(InMesh, OverrideBlendOutTime);
		}
	}
}

void UAbilityBase::MontageStopForAllMeshes(float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UAbilitySystemComponentBase* const AbilitySystemComponent = Cast<UAbilitySystemComponentBase>(CurrentActorInfo->AbilitySystemComponent.Get());
	if (AbilitySystemComponent != nullptr)
	{
		if (AbilitySystemComponent->IsAnimatingAbilityForAnyMesh(this))
		{
			AbilitySystemComponent->StopAllCurrentMontages(OverrideBlendOutTime);
		}
	}
}

AActor* UAbilityBase::GetOwnerActor(TSubclassOf<AActor> InClass /*= nullptr*/) const
{
	return GetDeterminesOutputObject(GetOwningActorFromActorInfo(), InClass);
}

TArray<TSubclassOf<UEffectBase>> UAbilityBase::GetEffectClasses() const
{
	TArray<TSubclassOf<UEffectBase>> ReturnValues;
	for(auto& Iter : EffectContainerMap)
	{
		ReturnValues.Append(Iter.Value.TargetGameplayEffectClasses);
	}
	return ReturnValues;
}
