#include "Ability/Abilities/AbilityBase.h"

#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Common/CommonModuleTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UAbilityBase::UAbilityBase()
{
	// Default to Instance Per Actor
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	bActivateOnInput = true;

	EffectContainerMap = TMap<FGameplayTag, FGameplayEffectContainer>();
}

void UAbilityBase::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
}

void UAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ActionRequest = FAbilityActionRequest();
	ActionActivationId.Invalidate();
	ActionOutcome = EAbilityActionOutcome::None;
	ActionReason = FGameplayTag();
	ActionFailureTags.Reset();
	bEndingAction = false;
	UAbilitySystemComponentBase* AbilitySystem = ActorInfo ? Cast<UAbilitySystemComponentBase>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	if (!AbilitySystem || !FAbilityActionTargetData::Read(TriggerEventData, ActionActivationId, ActionRequest))
	{
		ActionOutcome = EAbilityActionOutcome::Rejected;
		ActionReason = AbilityActionTags::InvalidRequest;
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	ActionRequest.AbilityHandle = Handle;
	if (!AbilitySystem->TrackActionActivation(ActionActivationId, Handle))
	{
		ActionOutcome = EAbilityActionOutcome::Rejected;
		ActionReason = AbilityActionTags::InvalidRequest;
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}
	const bool bExecuteGameplay = AbilitySystem->IsOwnerActorAuthoritative() || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTag Rejection;
	if (!AbilitySystem->ValidateGrantedRequest(ActionRequest, Rejection, bExecuteGameplay) || (bExecuteGameplay && !ValidateActionRequest(ActionRequest, Rejection)))
	{
		ActionOutcome = EAbilityActionOutcome::Rejected;
		ActionReason = Rejection;
		EndAbility(Handle, ActorInfo, ActivationInfo, bExecuteGameplay, true);
		return;
	}
	const FGuid ActivatingId = ActionActivationId;
	if (bExecuteGameplay)
	{
		const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo, &ActionFailureTags);
		if (!IsActive() || ActionActivationId != ActivatingId)
		{
			return;
		}
		if (!bCommitted)
		{
			ActionOutcome = EAbilityActionOutcome::Rejected;
			ActionReason = AbilityActionTags::ActivationFailed;
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}
	BeginAction();
}

void UAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEndingAction || !IsActive())
	{
		return;
	}
	bEndingAction = true;
	FAbilityActionResult Result;
	Result.ActivationId = ActionActivationId;
	Result.AbilityHandle = Handle;
	Result.Outcome = ActionOutcome != EAbilityActionOutcome::None ? ActionOutcome : (bWasCancelled ? EAbilityActionOutcome::Cancelled : EAbilityActionOutcome::Completed);
	Result.Reason = ActionReason;
	Result.FailureTags = ActionFailureTags;
	if (Result.Outcome == EAbilityActionOutcome::Cancelled && !Result.Reason.IsValid())
	{
		Result.Reason = AbilityActionTags::Cancelled;
	}
	TWeakObjectPtr<UAbilitySystemComponentBase> AbilitySystem = ActorInfo ? Cast<UAbilitySystemComponentBase>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const bool bCanReplicateEnd = AbilitySystem.IsValid() && (AbilitySystem->IsOwnerActorAuthoritative() || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility && bCanReplicateEnd, bWasCancelled);
	bEndingAction = false;
	if (IsActive() && ActionActivationId == Result.ActivationId)
	{
		return;
	}
	if (ActionActivationId == Result.ActivationId)
	{
		ActionActivationId.Invalidate();
		ActionRequest = FAbilityActionRequest();
	}
	if (AbilitySystem.IsValid() && Result.ActivationId.IsValid() && AbilitySystem->IsOwnerActorAuthoritative())
	{
		AbilitySystem->PublishActionResult(Result);
	}
	else if (AbilitySystem.IsValid() && Result.ActivationId.IsValid())
	{
		AbilitySystem->NotifyActionInstanceEnded(Result.ActivationId);
	}
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
	for (FAbilityMeshMontage& Entry : CurrentAbilityMeshMontages)
	{
		if (Entry.Mesh == InMesh)
		{
			Entry.Montage = InCurrentMontage;
			return;
		}
	}
	if (InMesh && InCurrentMontage)
	{
		CurrentAbilityMeshMontages.Emplace(InMesh, InCurrentMontage);
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


const FAbilityActionRequest& UAbilityBase::GetActionRequest() const
{
	return ActionRequest;
}

FGuid UAbilityBase::GetActionActivationId() const
{
	return ActionActivationId;
}

void UAbilityBase::CompleteAction()
{
	if (IsActive())
	{
		ActionOutcome = EAbilityActionOutcome::Completed;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UAbilityBase::FailAction(FGameplayTag Reason)
{
	if (IsActive())
	{
		ActionOutcome = EAbilityActionOutcome::Failed;
		ActionReason = Reason;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool UAbilityBase::RequestActionCancellation(FGameplayTag Reason)
{
	if (!IsActive() || !CanBeCanceled() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return false;
	}
	ActionOutcome = EAbilityActionOutcome::Cancelled;
	ActionReason = Reason.IsValid() ? Reason : AbilityActionTags::Cancelled;
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	return !IsActive();
}

bool UAbilityBase::ValidateActionRequest(const FAbilityActionRequest& Request, FGameplayTag& OutReason) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar) || !Request.ActionTag.IsValid() || Request.ActionTag != ActionTag || Request.TargetLocation.ContainsNaN() || Request.AimDirection.ContainsNaN() || Request.AimDirection.IsNearlyZero() || Request.WeaponPart < 0 || Request.WeaponPart > 255 || Request.ComboIndex < INDEX_NONE || Request.ComboIndex > 255)
	{
		OutReason = AbilityActionTags::InvalidRequest;
		return false;
	}
	if (Request.TargetActor && (!IsValid(Request.TargetActor) || Request.TargetActor->GetWorld() != Avatar->GetWorld()))
	{
		OutReason = AbilityActionTags::InvalidTarget;
		return false;
	}
	if (TargetPolicy == EAbilityActionTargetPolicy::Actor && !IsValid(Request.TargetActor))
	{
		OutReason = AbilityActionTags::InvalidTarget;
		return false;
	}
	if (TargetPolicy == EAbilityActionTargetPolicy::None)
	{
		return true;
	}
	const FVector Target = TargetPolicy == EAbilityActionTargetPolicy::Actor ? Request.TargetActor->GetActorLocation() : Request.TargetLocation;
	if (!FMath::IsFinite(MaximumTargetRange) || MaximumTargetRange <= 0.f || FVector::DistSquared(Avatar->GetActorLocation(), Target) > FMath::Square(MaximumTargetRange))
	{
		OutReason = AbilityActionTags::OutOfRange;
		return false;
	}
	if (bRequireLineOfSight)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AbilityActionTarget), false, Avatar);
		if (Avatar->GetWorld()->LineTraceSingleByChannel(Hit, Avatar->GetActorLocation(), Target, TargetTraceChannel, Params) && Hit.GetActor() != Request.TargetActor)
		{
			OutReason = AbilityActionTags::Blocked;
			return false;
		}
	}
	return true;
}

void UAbilityBase::BeginAction_Implementation()
{
	CompleteAction();
}

void UAbilityBase::ApplyCost_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
}

bool UAbilityBase::ValidateReplicatedTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag& OutReason) const
{
	if (!IsActive() || Data.Num() != 1 || !Data.Get(0) || Data.Get(0)->GetScriptStruct() != FGameplayAbilityTargetData_ActorArray::StaticStruct() || !IsValid(ActionRequest.TargetActor))
	{
		OutReason = AbilityActionTags::InvalidTarget;
		return false;
	}
	const TArray<TWeakObjectPtr<AActor>> Actors = Data.Get(0)->GetActors();
	if (Actors.Num() != 1 || Actors[0].Get() != ActionRequest.TargetActor.Get())
	{
		OutReason = AbilityActionTags::InvalidTarget;
		return false;
	}
	return ValidateActionRequest(ActionRequest, OutReason);
}

void UAbilityBase::ResolveAuthoritativeResult(const FAbilityActionResult& Result)
{
	if (!IsActive() || GetActionActivationId() != Result.ActivationId || CurrentSpecHandle != Result.AbilityHandle)
	{
		return;
	}
	ActionOutcome = Result.Outcome;
	ActionReason = Result.Reason;
	ActionFailureTags = Result.FailureTags;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, Result.Outcome != EAbilityActionOutcome::Completed);
}
