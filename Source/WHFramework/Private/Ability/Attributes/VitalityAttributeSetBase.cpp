#include "Ability/Attributes/VitalityAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Net/UnrealNetwork.h"
#include "Ability/AbilityModuleTypes.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"

UVitalityAttributeSetBase::UVitalityAttributeSetBase()
:	Exp(0.f),
	MaxExp(50.f),
	Health(50.f),
	MaxHealth(50.f),
	PhysicsDamage(0.f),
	MagicDamage(0.f)
{
	DamageHandle = UDamageHandle::StaticClass();
}

void UVitalityAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(AbilityComp->GetAvatarActor());
	
	const float CurrentValue = Attribute.GetGameplayAttributeData(this)->GetCurrentValue();
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
}

void UVitalityAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* Source = Context.GetOriginalInstigatorAbilitySystemComponent();
	const FGameplayTagContainer& SourceTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();

	AActor* TargetActor = nullptr;
	IAbilityVitalityInterface* TargetVitality = nullptr;
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);
	}
	
	if (Data.EvaluatedData.Attribute.GetName().EndsWith("Damage"))
	{
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

		if(Data.EvaluatedData.Attribute.GetName().StartsWith("Physics"))
		{
			UReferencePoolModuleStatics::GetReference<UDamageHandle>(true, DamageHandle).HandleDamage(SourceActor, TargetActor, GetPhysicsDamage(), EDamageType::Physics, HitResult, SourceTags);
			SetPhysicsDamage(0.f);
		}
		else if(Data.EvaluatedData.Attribute.GetName().StartsWith("Magic"))
		{
			UReferencePoolModuleStatics::GetReference<UDamageHandle>(true, DamageHandle).HandleDamage(SourceActor, TargetActor, GetMagicDamage(), EDamageType::Magic, HitResult, SourceTags);
			SetMagicDamage(0.f);
		}
	}
}

void UVitalityAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, Exp, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, MaxExp, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, PhysicsDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVitalityAttributeSetBase, MagicDamage, COND_None, REPNOTIFY_Always);
}

void UVitalityAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, Health, OldHealth);
}

void UVitalityAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, MaxHealth, OldMaxHealth);
}

void UVitalityAttributeSetBase::OnRep_Exp(const FGameplayAttributeData& OldExp)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, Exp, OldExp);
}

void UVitalityAttributeSetBase::OnRep_MaxExp(const FGameplayAttributeData& OldMaxExp)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, MaxExp, OldMaxExp);
}

void UVitalityAttributeSetBase::OnRep_PhysicsDamage(const FGameplayAttributeData& OldPhysicsDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, PhysicsDamage, OldPhysicsDamage);
}

void UVitalityAttributeSetBase::OnRep_MagicDamage(const FGameplayAttributeData& OldMagicDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVitalityAttributeSetBase, MagicDamage, OldMagicDamage);
}

