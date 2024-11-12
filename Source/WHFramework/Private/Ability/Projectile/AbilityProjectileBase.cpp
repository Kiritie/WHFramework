// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Projectile/AbilityProjectileBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

AAbilityProjectileBase::AAbilityProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;

	OriginSocketName = NAME_None;
	FinalSocketName = NAME_None;
	MaxDurationTime = 0.f;
	OwnerActor = nullptr;
	AbilityLevel = 0;
	EffectContainerMap = TMap<FGameplayTag, FGameplayEffectContainer>();
	HitTargets = TArray<AActor*>();
	bLaunched = false;
}

void AAbilityProjectileBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	OwnerActor = Cast<AActor>(InOwner);

	if(OwnerActor && InParams.IsValidIndex(0))
	{
		if(UMeshComponent* MeshComponent = OwnerActor->FindComponentByClass<UMeshComponent>())
		{
			AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, OriginSocketName);
			SetActorRotation(FinalSocketName.IsNone() ? OwnerActor->GetActorRotation() : UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), MeshComponent->GetSocketLocation(FinalSocketName)));
		}

		if(UAbilitySystemComponentBase* OwningASC = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(OwnerActor))
		{
			const FGameplayAbilitySpec Spec = OwningASC->FindAbilitySpecForHandle(InParams[0].GetPointerValueRef<FGameplayAbilitySpecHandle>());

			if(UAbilityBase* Ability = Cast<UAbilityBase>(Spec.GetPrimaryInstance()))
			{
				AbilityLevel = Ability->GetAbilityLevel();
				AbilityActorInfo = Ability->GetActorInfo();
				EffectContainerMap = Ability->EffectContainerMap;
			}
		}
	}
}

void AAbilityProjectileBase::OnDespawn_Implementation(bool bRecovery)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	Super::OnDespawn_Implementation(bRecovery);

	SetHitAble(false);
	ClearHitTargets();

	OwnerActor = nullptr;
	AbilityLevel = 0;
	AbilityActorInfo = FGameplayAbilityActorInfo();
	EffectContainerMap.Empty();
	bLaunched = false;
}

void AAbilityProjectileBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AAbilityProjectileBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(UMeshComponent* MeshComponent = Cast<UMeshComponent>(RootComponent->GetAttachParent()))
	{
		SetActorRotation(FinalSocketName.IsNone() ? OwnerActor->GetActorRotation() : UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), MeshComponent->GetSocketLocation(FinalSocketName)));
	}
}

void AAbilityProjectileBase::Launch_Implementation(FVector InDirection)
{
	bLaunched = true;
	
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	SetActorRotation(InDirection != FVector::ZeroVector ? InDirection.Rotation() : OwnerActor->GetActorRotation());

	SetHitAble(true);

	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, [this](){
		Destroy();
	}, MaxDurationTime, false);
}

void AAbilityProjectileBase::Destroy_Implementation()
{
	UObjectPoolModuleStatics::DespawnObject(this);

	GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
}

bool AAbilityProjectileBase::CanHitTarget(AActor* InTarget) const
{
	return InTarget != OwnerActor && !HitTargets.Contains(InTarget);
}

void AAbilityProjectileBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	HitTargets.Add(InTarget);
	
	FGameplayEventData EventData;
	EventData.Instigator = OwnerActor;
	EventData.Target = InTarget;
	EventData.OptionalObject = this;

	ApplyEffectContainer(HitEventTag, EventData, -1);
}

bool AAbilityProjectileBase::IsHitAble() const
{
	return true;
}

void AAbilityProjectileBase::SetHitAble(bool bValue)
{

}

void AAbilityProjectileBase::ClearHitTargets()
{
	HitTargets.Empty();
}

TArray<AActor*> AAbilityProjectileBase::GetHitTargets() const
{
	return HitTargets;
}

FGameplayEffectContainerSpec AAbilityProjectileBase::MakeEffectContainerSpecFromContainer(const FGameplayEffectContainer& InContainer, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	// First figure out our actor info
	FGameplayEffectContainerSpec ReturnSpec;

	if (UAbilitySystemComponentBase* OwningASC = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(OwnerActor))
	{
		// If we have a target type, run the targeting logic. This is optional, targets can be added later
		if (InContainer.TargetType.Get())
		{
			TArray<FHitResult> HitResults;
			TArray<AActor*> TargetActors;
			const UTargetType* TargetType = InContainer.TargetType.GetDefaultObject();
			AActor* AvatarActor = AbilityActorInfo.AvatarActor.Get();
			TargetType->GetTargets(OwnerActor, AvatarActor, EventData, HitResults, TargetActors);
			ReturnSpec.AddTargets(HitResults, TargetActors);
		}
		// If we don't have an override level, use the ability level
		if (OverrideGameplayLevel == INDEX_NONE)
		{
			//OverrideGameplayLevel = OwningASC->GetDefaultAbilityLevel();
			OverrideGameplayLevel = AbilityLevel;
		}
		// Build GameplayEffectSpecs for each applied effect
		for (const TSubclassOf<UEffectBase>& EffectClass : InContainer.TargetGameplayEffectClasses)
		{
			auto EffectContext = OwningASC->MakeEffectContext();
			EffectContext.AddSourceObject(this);
			ReturnSpec.TargetGameplayEffectSpecs.Add(OwningASC->MakeOutgoingSpec(EffectClass, OverrideGameplayLevel, EffectContext));
		}
	}
	return ReturnSpec;
}

FGameplayEffectContainerSpec AAbilityProjectileBase::MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	if (FGameplayEffectContainer* FoundContainer = EffectContainerMap.Find(ContainerTag))
	{
		return MakeEffectContainerSpecFromContainer(*FoundContainer, EventData, OverrideGameplayLevel);
	}
	return FGameplayEffectContainerSpec();
}

TArray<FActiveGameplayEffectHandle> AAbilityProjectileBase::ApplyEffectContainerSpec(const FGameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;

	// Iterate list of effect specs and apply them to their target data
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		AllEffects.Append(ApplyGameplayEffectSpecToTarget(SpecHandle, ContainerSpec.TargetData));
	}
	return AllEffects;
}

TArray<FActiveGameplayEffectHandle> AAbilityProjectileBase::ApplyEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	FGameplayEffectContainerSpec Spec = MakeEffectContainerSpec(ContainerTag, EventData, OverrideGameplayLevel);
	return ApplyEffectContainerSpec(Spec);
}
 
TArray<FActiveGameplayEffectHandle> AAbilityProjectileBase::ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData) const
{
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	UAbilitySystemComponentBase* OwningASC = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(OwnerActor);

	if (SpecHandle.IsValid() && OwningASC)
	{
		for (TSharedPtr<FGameplayAbilityTargetData> Data : TargetData.Data)
		{
			if (Data.IsValid())
			{
				EffectHandles.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get(), OwningASC->GetPredictionKeyForNewAction()));
			}
			else
			{
				WHLog(FString::Printf(TEXT("ApplyGameplayEffectSpecToTarget invalid target data passed in. Ability: %s"), *GetPathName()), EDC_Ability, EDV_Warning);
			}
		}
	}
	return EffectHandles;
}
