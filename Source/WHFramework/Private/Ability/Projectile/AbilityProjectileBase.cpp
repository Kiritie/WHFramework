// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Projectile/AbilityProjectileBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemLog.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "AI/Base/AIBlackboardBase.h"
#include "Camera/CameraModuleStatics.h"
#include "Common/Looking/LookingComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

AAbilityProjectileBase::AAbilityProjectileBase()
{
	DurationTime = 0;
	SocketName = NAME_None;
	OwnerActor = nullptr;
	HitTargets = TArray<AActor*>();
}

void AAbilityProjectileBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void AAbilityProjectileBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	SetHitAble(false);
	ClearHitTargets();

	GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
}

void AAbilityProjectileBase::Initialize_Implementation(AActor* InOwnerActor, const FGameplayAbilitySpecHandle& InAbilityHandle)
{
	OwnerActor = InOwnerActor;

	if(OwnerActor)
	{
		if(AAbilityCharacterBase* AbilityCharacter = GetOwnerActor<AAbilityCharacterBase>())
		{
			SetActorLocation(AbilityCharacter->GetMesh()->GetSocketLocation(SocketName));
			if(!AbilityCharacter->IsPlayer())
			{
				SetActorRotation(AbilityCharacter->GetLooking()->GetLookingRotation(AbilityCharacter->GetBlackboard()->GetTargetAgent<AActor>()));
			}
			else
			{
				SetActorRotation(UCameraModuleStatics::GetCameraRotation(true));
			}
		}
		else if(UMeshComponent* MeshComponent = OwnerActor->FindComponentByClass<UMeshComponent>())
		{
			SetActorLocation(MeshComponent->GetSocketLocation(SocketName));
			SetActorRotation(InOwnerActor->GetActorRotation());
		}

		UAbilitySystemComponentBase* OwningASC = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(OwnerActor);

		const FGameplayAbilitySpec Spec = OwningASC->FindAbilitySpecForHandle(InAbilityHandle);
		if(UAbilityBase* Ability = Cast<UAbilityBase>(Spec.GetPrimaryInstance()))
		{
			EffectContainerMap = Ability->EffectContainerMap;
			Level = Ability->GetAbilityLevel();
		}
	}

	SetHitAble(true);

	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, [this](){
		UObjectPoolModuleStatics::DespawnObject(this);
	}, DurationTime, false);
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

	ApplyEffectContainer(HitEventTag, EventData, 1);
}

void AAbilityProjectileBase::ClearHitTargets()
{
	HitTargets.Empty();
}

void AAbilityProjectileBase::SetHitAble(bool bValue)
{

}

FGameplayEffectContainerSpec AAbilityProjectileBase::MakeEffectContainerSpecFromContainer(const FGameplayEffectContainer& InContainer, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	// First figure out our actor info
	FGameplayEffectContainerSpec ReturnSpec;
	UAbilitySystemComponentBase* OwningASC = UAbilitySystemComponentBase::GetAbilitySystemComponentFormActor(OwnerActor);

	if (OwningASC)
	{
		// If we have a target type, run the targeting logic. This is optional, targets can be added later
		if (InContainer.TargetType.Get())
		{
			TArray<FHitResult> HitResults;
			TArray<AActor*> TargetActors;
			const UTargetType* TargetType = InContainer.TargetType.GetDefaultObject();
			TargetType->GetTargets(OwnerActor, OwnerActor, EventData, HitResults, TargetActors);
			ReturnSpec.AddTargets(HitResults, TargetActors);
		}
		// If we don't have an override level, use the ability level
		if (OverrideGameplayLevel == INDEX_NONE)
		{
			//OverrideGameplayLevel = OwningASC->GetDefaultAbilityLevel();
			OverrideGameplayLevel = Level;
		}
		// Build GameplayEffectSpecs for each applied effect
		for (const TSubclassOf<UGameplayEffect>& EffectClass : InContainer.TargetGameplayEffectClasses)
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
	FGameplayEffectContainer* FoundContainer = EffectContainerMap.Find(ContainerTag);

	if (FoundContainer)
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

	if (SpecHandle.IsValid())
	{
		for (TSharedPtr<FGameplayAbilityTargetData> Data : TargetData.Data)
		{
			if (Data.IsValid())
			{
				EffectHandles.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get(), OwningASC->GetPredictionKeyForNewAction()));
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("UGameplayAbility::ApplyGameplayEffectSpecToTarget invalid target data passed in. Ability: %s"), *GetPathName());
			}
		}
	}
	return EffectHandles;
}
