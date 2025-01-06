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
	Interrupt(0.f)
{
	DamageHandleClass = UDamageHandle::StaticClass();
	RecoveryHandleClass = URecoveryHandle::StaticClass();
	InterruptHandleClass = UInterruptHandle::StaticClass();
}

void UVitalityAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UVitalityAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
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

void UVitalityAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* Source = Context.GetOriginalInstigatorAbilitySystemComponent();
	const FGameplayTagContainer& SourceTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();

	AActor* SourceActor = nullptr;
	if (Source && Source->AbilityActorInfo.IsValid() && Source->AbilityActorInfo->AvatarActor.IsValid())
	{
		SourceActor = Source->AbilityActorInfo->AvatarActor.Get();
	}
	FHitResult HitResult;
	if (Context.GetHitResult())
	{
		HitResult = *Context.GetHitResult();
	}

	AActor* TargetActor = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
	}

	if(Data.EvaluatedData.Attribute == GetHealthRecoveryAttribute())
	{
		UReferencePoolModuleStatics::GetReference<URecoveryHandle>(true, RecoveryHandleClass).HandleRecovery(SourceActor, TargetActor, GetHealthRecovery(), GetHealthRecoveryAttribute(), HitResult, SourceTags);
		SetHealthRecovery(0.f);
	}
	else if(Data.EvaluatedData.Attribute == GetPhysicsDamageAttribute())
	{
		UReferencePoolModuleStatics::GetReference<UDamageHandle>(true, DamageHandleClass).HandleDamage(SourceActor, TargetActor, GetPhysicsDamage(), GetPhysicsDamageAttribute(), HitResult, SourceTags);
		SetPhysicsDamage(0.f);
	}
	else if(Data.EvaluatedData.Attribute == GetMagicDamageAttribute())
	{
		UReferencePoolModuleStatics::GetReference<UDamageHandle>(true, DamageHandleClass).HandleDamage(SourceActor, TargetActor, GetMagicDamage(), GetMagicDamageAttribute(), HitResult, SourceTags);
		SetMagicDamage(0.f);
	}
	else if(Data.EvaluatedData.Attribute == GetFallDamageAttribute())
	{
		UReferencePoolModuleStatics::GetReference<UDamageHandle>(true, DamageHandleClass).HandleDamage(SourceActor, TargetActor, GetFallDamage(), GetFallDamageAttribute(), HitResult, SourceTags);
		SetFallDamage(0.f);
	}
	else if (Data.EvaluatedData.Attribute == GetInterruptAttribute())
	{
		UReferencePoolModuleStatics::GetReference<UInterruptHandle>(true, InterruptHandleClass).HandleInterrupt(SourceActor, TargetActor, GetInterrupt(), GetInterruptAttribute(), HitResult, SourceTags);
		SetInterrupt(0.f);
	}
}

void UVitalityAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, HealthRegenSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, PhysicsDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, MagicDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, FallDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, HealthRecovery, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, Interrupt, COND_None, REPNOTIFY_Always);
}

void UVitalityAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, Health, OldHealth);
}

void UVitalityAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, MaxHealth, OldMaxHealth);
}

void UVitalityAttributeSetBase::OnRep_HealthRecovery(const FGameplayAttributeData& OldRecovery)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, HealthRecovery, OldRecovery);
}

void UVitalityAttributeSetBase::OnRep_HealthRegenSpeed(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, HealthRegenSpeed, OldMaxHealth);
}

void UVitalityAttributeSetBase::OnRep_PhysicsDamage(const FGameplayAttributeData& OldPhysicsDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, PhysicsDamage, OldPhysicsDamage);
}

void UVitalityAttributeSetBase::OnRep_MagicDamage(const FGameplayAttributeData& OldMagicDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, MagicDamage, OldMagicDamage);
}

void UVitalityAttributeSetBase::OnRep_FallDamage(const FGameplayAttributeData& OldFallDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, FallDamage, OldFallDamage);
}

void UVitalityAttributeSetBase::OnRep_Interrupt(const FGameplayAttributeData& OldInterrupt)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, Interrupt, OldInterrupt);
}
