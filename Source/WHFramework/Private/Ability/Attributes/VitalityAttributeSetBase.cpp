#include "Ability/Attributes/VitalityAttributeSetBase.h"

#include "GameplayEffectExtension.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "Net/UnrealNetwork.h"

UVitalityAttributeSetBase::UVitalityAttributeSetBase()
:	Health(100.f),
	MaxHealth(100.f),
	Exp(0.f),
	MaxExp(100.f),
	PhysicsDamage(0.f),
	MagicDamage(0.f)
{
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
	AAbilityCharacterBase* TargetCharacter = Cast<AAbilityCharacterBase>(TargetActor);
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);
		TargetCharacter = Cast<AAbilityCharacterBase>(TargetActor);
	}
	
	if (Data.EvaluatedData.Attribute.GetName().EndsWith("Damage"))
	{
		float LocalDamageDone = 0.f;
		
		AActor* SourceActor = nullptr;
		AAbilityCharacterBase* SourceCharacter = nullptr;
		if (Source && Source->AbilityActorInfo.IsValid() && Source->AbilityActorInfo->AvatarActor.IsValid())
		{
			SourceActor = Source->AbilityActorInfo->AvatarActor.Get();
			SourceCharacter = Cast<AAbilityCharacterBase>(SourceActor);
		}
		if(Data.EvaluatedData.Attribute.GetName().StartsWith("Physics"))
		{
			LocalDamageDone = GetPhysicsDamage();
			SetPhysicsDamage(0.f);
		}
		else if(Data.EvaluatedData.Attribute.GetName().StartsWith("Magic"))
		{
			LocalDamageDone = GetMagicDamage();
			SetMagicDamage(0.f);
		}

		if (LocalDamageDone > 0.f)
		{
			if (TargetVitality && !TargetVitality->IsDead())
			{
				TargetVitality->ModifyHealth(-LocalDamageDone);

				FHitResult HitResult;
				if (Context.GetHitResult())
				{
					HitResult = *Context.GetHitResult();
				}
				TargetVitality->HandleDamage(EDamageType::Physics, LocalDamageDone, true, HitResult, SourceTags, SourceActor);
			}
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

