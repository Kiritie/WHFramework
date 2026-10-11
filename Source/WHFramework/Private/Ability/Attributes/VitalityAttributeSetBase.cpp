#include "Ability/Attributes/VitalityAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Ability/AbilityModuleTypes.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"

UVitalityAttributeSetBase::UVitalityAttributeSetBase()
:	Health(50.f),
	MaxHealth(50.f),
	HealthRecovery(0.f),
	HealthRegenSpeed(0.f),
	PhysicsDamage(0.f),
	MagicDamage(0.f),
	FallDamage(0.f),
	RealDamage(0.f),
	Interrupt(0.f)
{
	DamageHandleClass = UDamageHandle::StaticClass();
	RecoveryHandleClass = URecoveryHandle::StaticClass();
	InterruptHandleClass = UInterruptHandle::StaticClass();
}

void UVitalityAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(1.f, GetMaxHealth()));
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
	else if (Attribute.GetAttributeSetClass() == StaticClass())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}

void UVitalityAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustAttributeForMaxChange(GetHealthAttribute(), OldValue, NewValue);
	}
}

void UVitalityAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (!Data.Target.IsOwnerActorAuthoritative())
	{
		return;
	}
	const FGameplayAttribute Changed = Data.EvaluatedData.Attribute;
	if (Changed == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, FMath::Max(1.f, GetMaxHealth())));
		return;
	}
	float Magnitude = 0.f;
	if (Changed == GetHealthRecoveryAttribute())
	{
		Magnitude = GetHealthRecovery();
		SetHealthRecovery(0.f);
	}
	else if (Changed == GetPhysicsDamageAttribute())
	{
		Magnitude = GetPhysicsDamage();
		SetPhysicsDamage(0.f);
	}
	else if (Changed == GetMagicDamageAttribute())
	{
		Magnitude = GetMagicDamage();
		SetMagicDamage(0.f);
	}
	else if (Changed == GetFallDamageAttribute())
	{
		Magnitude = GetFallDamage();
		SetFallDamage(0.f);
	}
	else if (Changed == GetRealDamageAttribute())
	{
		Magnitude = GetRealDamage();
		SetRealDamage(0.f);
	}
	else if (Changed == GetInterruptAttribute())
	{
		Magnitude = GetInterrupt();
		SetInterrupt(0.f);
	}
	if (!FMath::IsFinite(Magnitude) || Magnitude <= 0.f)
	{
		return;
	}
	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
	AActor* SourceActor = Context.GetEffectCauser();
	AActor* TargetActor = Data.Target.GetAvatarActor();
	const FGameplayTagContainer* CapturedTags = Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer SourceTags = CapturedTags ? *CapturedTags : FGameplayTagContainer();
	const FHitResult Hit = Context.GetHitResult() ? *Context.GetHitResult() : FHitResult();
	if (!IsValid(TargetActor))
	{
		return;
	}
	if (Changed == GetHealthRecoveryAttribute() && RecoveryHandleClass)
	{
		UReferencePoolModuleStatics::GetDefaultReference<URecoveryHandle>(RecoveryHandleClass).HandleRecovery(SourceActor, TargetActor, Magnitude, Changed, Hit, SourceTags);
	}
	else if (Changed == GetInterruptAttribute() && InterruptHandleClass)
	{
		UReferencePoolModuleStatics::GetDefaultReference<UInterruptHandle>(InterruptHandleClass).HandleInterrupt(SourceActor, TargetActor, Magnitude, Changed, Hit, SourceTags);
	}
	else if (DamageHandleClass)
	{
		UReferencePoolModuleStatics::GetDefaultReference<UDamageHandle>(DamageHandleClass).HandleDamage(SourceActor, TargetActor, Magnitude, Changed, Hit, SourceTags);
	}
}

void UVitalityAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, HealthRegenSpeed, COND_None, REPNOTIFY_Always);
}

void UVitalityAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, Health, OldHealth);
}

void UVitalityAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, MaxHealth, OldMaxHealth);
}



void UVitalityAttributeSetBase::OnRep_HealthRegenSpeed(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, HealthRegenSpeed, OldMaxHealth);
}
