// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Projectile/AbilityProjectileBase.h"

#include "TimerManager.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Effects/EffectBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

AAbilityProjectileBase::AAbilityProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	OriginSocketName = NAME_None;
	FinalSocketName = NAME_None;
	MaxDurationTime = 10.f;
	OwnerActor = nullptr;
	AbilityLevel = 0;
	EffectContainerMap = TMap<FGameplayTag, FGameplayEffectContainer>();
	HitTargets = TArray<AActor*>();
	bLaunched = false;
}

void AAbilityProjectileBase::OnSpawn_Implementation(const FParameter& InParam)
{
	Super::OnSpawn_Implementation(InParam);
	SetHitAble(false);
	ClearHitTargets();
	bLaunched = false;
	bDestroyingProjectile = false;
	PreparedEffects.Reset();
	ProjectileContext = FGameplayEffectContextHandle();
	SourceActivationId.Invalidate();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
	}
	if (!HasAuthority())
	{
		return;
	}
	const FAbilityProjectileSpawnParameter* Param = InParam.GetPtr<FAbilityProjectileSpawnParameter>();
	OwnerActor = Param ? Param->Owner.Get() : nullptr;
	IAbilityActorInterface* AbilityActor = Cast<IAbilityActorInterface>(OwnerActor);
	UAbilitySystemComponentBase* SourceSystem = AbilityActor ? Cast<UAbilitySystemComponentBase>(AbilityActor->GetAbilitySystemComponent()) : nullptr;
	const FGameplayAbilitySpec* Spec = SourceSystem && Param ? SourceSystem->FindAbilitySpecFromHandle(Param->AbilityHandle) : nullptr;
	UAbilityBase* Action = Spec ? Cast<UAbilityBase>(Spec->GetPrimaryInstance()) : nullptr;
	FGameplayTag Reason;
	if (!IsValid(OwnerActor) || !Action || !Action->IsActive() || !SourceSystem->IsOwnerActorAuthoritative() || SourceSystem->GetAvatarActor() != OwnerActor || !SourceSystem->ValidateGrantedRequest(Action->GetActionRequest(), Reason))
	{
		Destroy();
		return;
	}
	SetOwner(OwnerActor);
	SetActorHiddenInGame(false);
	SourceActivationId = Action->GetActionActivationId();
	AbilityLevel = Action->GetAbilityLevel();
	EffectContainerMap = Action->EffectContainerMap;
	ProjectileContext = SourceSystem->MakeEffectContext();
	ProjectileContext.AddSourceObject(Spec->SourceObject.Get());
	for (const TPair<FGameplayTag, FGameplayEffectContainer>& Entry : EffectContainerMap)
	{
		for (TSubclassOf<UEffectBase> EffectClass : Entry.Value.TargetGameplayEffectClasses)
		{
			if (EffectClass && !PreparedEffects.Contains(EffectClass))
			{
				PreparedEffects.Add(EffectClass, SourceSystem->MakeOutgoingSpec(EffectClass, AbilityLevel, ProjectileContext));
			}
		}
	}
	AbilityActor->AttachActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale, OriginSocketName);
	if (UMeshComponent* Mesh = AbilityActor->GetMeshComponent())
	{
		SetActorRotation(FinalSocketName.IsNone() ? OwnerActor->GetActorRotation() : UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Mesh->GetSocketLocation(FinalSocketName)));
	}
	const float Lifetime = FMath::IsFinite(MaxDurationTime) ? FMath::Clamp(MaxDurationTime, 0.01f, 120.f) : 10.f;
	GetWorld()->GetTimerManager().SetTimer(DestroyTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { Destroy(); }), Lifetime, false);
	ForceNetUpdate();
}

void AAbilityProjectileBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
	}
	SetHitAble(false);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	ClearHitTargets();
	PreparedEffects.Reset();
	ProjectileContext = FGameplayEffectContextHandle();
	SourceActivationId.Invalidate();
	OwnerActor = nullptr;
	SetOwner(nullptr);
	AbilityLevel = 0;
	EffectContainerMap.Reset();
	bLaunched = false;
	bDestroyingProjectile = true;
	Super::OnDespawn_Implementation(InMode);
}

void AAbilityProjectileBase::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AAbilityProjectileBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
	if (HasAuthority() && !bLaunched && IsValid(OwnerActor) && RootComponent)
	{
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(RootComponent->GetAttachParent()))
		{
			SetActorRotation(FinalSocketName.IsNone() ? OwnerActor->GetActorRotation() : UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Mesh->GetSocketLocation(FinalSocketName)));
		}
	}
}

void AAbilityProjectileBase::Launch_Implementation(FVector InDirection)
{
	if (!HasAuthority() || bLaunched || bDestroyingProjectile || !SourceActivationId.IsValid() || !IsValid(OwnerActor) || InDirection.ContainsNaN())
	{
		return;
	}
	bLaunched = true;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(InDirection.IsNearlyZero() ? OwnerActor->GetActorRotation() : InDirection.GetSafeNormal().Rotation());
	SetHitAble(true);
	ForceNetUpdate();
}

void AAbilityProjectileBase::Destroy_Implementation()
{
	if (!HasAuthority() || bDestroyingProjectile)
	{
		return;
	}
	bDestroyingProjectile = true;
	SetHitAble(false);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DestroyTimer);
	}
	UObjectPoolModuleStatics::DespawnObject(this);
}

bool AAbilityProjectileBase::CanHitTarget(AActor* InTarget) const
{
	return HasAuthority() && IsHitAble() && IsValid(InTarget) && InTarget->GetWorld() == GetWorld() && InTarget != this && InTarget != OwnerActor && !HitTargets.Contains(InTarget);
}

void AAbilityProjectileBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	if (!CanHitTarget(InTarget))
	{
		return;
	}
	HitTargets.Add(InTarget);
	FHitResult Hit = InHitResult;
	if (Hit.GetActor() != InTarget)
	{
		Hit = FHitResult(InTarget, Cast<UPrimitiveComponent>(InTarget->GetRootComponent()), InTarget->GetActorLocation(), -GetActorForwardVector());
	}
	FGameplayEventData Event;
	Event.EventTag = HitEventTag;
	Event.Instigator = OwnerActor;
	Event.Target = InTarget;
	Event.OptionalObject = this;
	Event.ContextHandle = ProjectileContext.Duplicate();
	Event.ContextHandle.AddHitResult(Hit, true);
	ApplyEffectContainer(HitEventTag, Event, INDEX_NONE);
	Destroy();
}

bool AAbilityProjectileBase::IsHitAble() const
{
	return HasAuthority() && bLaunched && bHitEnabled && !bDestroyingProjectile;
}

void AAbilityProjectileBase::SetHitAble(bool bValue)
{
	bHitEnabled = bValue && HasAuthority() && bLaunched && !bDestroyingProjectile;
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
	FGameplayEffectContainerSpec Result;
	if (!HasAuthority() || (OverrideGameplayLevel != INDEX_NONE && OverrideGameplayLevel != AbilityLevel))
	{
		return Result;
	}
	TArray<FHitResult> Hits;
	TArray<AActor*> Targets;
	if (InContainer.TargetType)
	{
		InContainer.TargetType.GetDefaultObject()->GetTargets(OwnerActor, OwnerActor, EventData, Hits, Targets);
	}
	else if (IsValid(EventData.Target.Get()))
	{
		Targets.Add(const_cast<AActor*>(EventData.Target.Get()));
	}
	Targets.RemoveAll([this](const AActor* Target) { return !IsValid(Target) || Target->GetWorld() != GetWorld(); });
	Hits.RemoveAll([this](const FHitResult& Hit) { return !IsValid(Hit.GetActor()) || Hit.GetActor()->GetWorld() != GetWorld(); });
	Result.AddTargets(Hits, Targets);
	for (TSubclassOf<UEffectBase> EffectClass : InContainer.TargetGameplayEffectClasses)
	{
		const FGameplayEffectSpecHandle* Prepared = PreparedEffects.Find(EffectClass);
		if (!Prepared || !Prepared->IsValid())
		{
			continue;
		}
		FGameplayEffectSpecHandle Spec(new FGameplayEffectSpec(*Prepared->Data.Get()));
		FGameplayEffectContextHandle Context = Spec.Data->GetContext().Duplicate();
		if (Context.IsValid() && Context.Get()->GetScriptStruct() == FGameplayEffectContextBase::StaticStruct())
		{
			static_cast<FGameplayEffectContextBase*>(Context.Get())->EffectInstanceId = FGuid::NewGuid();
		}
		if (const FHitResult* Hit = EventData.ContextHandle.GetHitResult())
		{
			Context.AddHitResult(*Hit, true);
		}
		Spec.Data->SetContext(Context);
		Result.TargetGameplayEffectSpecs.Add(Spec);
	}
	return Result;
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
	TArray<FActiveGameplayEffectHandle> Applied;
	if (!HasAuthority() || !SpecHandle.IsValid())
	{
		return Applied;
	}
	TSet<AActor*> Visited;
	for (const TSharedPtr<FGameplayAbilityTargetData>& Data : TargetData.Data)
	{
		if (!Data.IsValid())
		{
			continue;
		}
		for (const TWeakObjectPtr<AActor>& Target : Data->GetActors())
		{
			if (!Target.IsValid() || Target->GetWorld() != GetWorld() || Visited.Contains(Target.Get()))
			{
				continue;
			}
			Visited.Add(Target.Get());
			UAbilitySystemComponent* TargetSystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target.Get());
			if (TargetSystem && TargetSystem->IsOwnerActorAuthoritative())
			{
				FGameplayEffectSpec PerTarget(*SpecHandle.Data.Get());
				FGameplayEffectContextHandle Context = PerTarget.GetContext().Duplicate();
				if (Context.IsValid() && Context.Get()->GetScriptStruct() == FGameplayEffectContextBase::StaticStruct())
				{
					static_cast<FGameplayEffectContextBase*>(Context.Get())->EffectInstanceId = FGuid::NewGuid();
				}
				PerTarget.SetContext(Context);
				Applied.Add(TargetSystem->ApplyGameplayEffectSpecToSelf(PerTarget));
			}
		}
	}
	return Applied;
}


void AAbilityProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityProjectileBase, OwnerActor);
	DOREPLIFETIME(AAbilityProjectileBase, bLaunched);
}
