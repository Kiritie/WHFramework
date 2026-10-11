#include "Ability/Attributes/AttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "Common/CommonModuleTypes.h"

UAttributeSetBase::UAttributeSetBase()
{
}

void UAttributeSetBase::SerializeAttributes(FArchive& Ar)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		return;
	}
	TArray<FGameplayAttribute> Attributes = GetPersistentAttributes();
	Attributes.Sort([](const FGameplayAttribute& A, const FGameplayAttribute& B)
	{
		return A.GetName() < B.GetName();
	});
	uint32 Format = 0x57484132;
	int32 Count = Attributes.Num();
	Ar << Format;
	Ar << Count;
	if (Format != 0x57484132 || Count < 0 || Count > 256)
	{
		Ar.SetError();
		return;
	}
	TMap<FName, float> Loaded;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FName Name = Ar.IsSaving() ? FName(*Attributes[Index].GetName()) : NAME_None;
		float Value = Ar.IsSaving() ? Attributes[Index].GetGameplayAttributeData(this)->GetBaseValue() : 0.f;
		Ar << Name;
		Ar << Value;
		if (Name.IsNone() || !FMath::IsFinite(Value) || Loaded.Contains(Name))
		{
			Ar.SetError();
			return;
		}
		Loaded.Add(Name, Value);
	}
	if (Ar.IsLoading() && !Ar.IsError())
	{
		UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent();
		if (!AbilitySystem || !AbilitySystem->IsOwnerActorAuthoritative())
		{
			Ar.SetError();
			return;
		}
		Attributes.StableSort([](const FGameplayAttribute& A, const FGameplayAttribute& B)
		{
			return A.GetName().StartsWith(TEXT("Max")) && !B.GetName().StartsWith(TEXT("Max"));
		});
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			if (const float* Value = Loaded.Find(FName(*Attribute.GetName())))
			{
				AbilitySystem->SetNumericAttributeBase(Attribute, *Value);
			}
		}
	}
}

void UAttributeSetBase::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	const_cast<ThisClass*>(this)->PreAttributeChange(Attribute, NewValue);
}

void UAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (!FMath::IsFinite(NewValue))
	{
		NewValue = 0.f;
	}
}

void UAttributeSetBase::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
}

void UAttributeSetBase::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
}

void UAttributeSetBase::AdjustAttributeForMaxChange(const FGameplayAttribute& Attribute, float OldMaxValue, float NewMaxValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const FGameplayAttributeData* Data = Attribute.GetGameplayAttributeData(this);
	if (!AbilityComp || !AbilityComp->IsOwnerActorAuthoritative() || !Data || !FMath::IsFinite(OldMaxValue) || !FMath::IsFinite(NewMaxValue) || FMath::IsNearlyEqual(OldMaxValue, NewMaxValue))
	{
		return;
	}
	const float Current = Data->GetCurrentValue();
	const float NewMaximum = FMath::Max(NewMaxValue, 0.f);
	const float Next = OldMaxValue > UE_SMALL_NUMBER ? FMath::Clamp(Current / OldMaxValue * NewMaximum, 0.f, NewMaximum) : FMath::Clamp(Current, 0.f, NewMaximum);
	ModifyAttributeValue(Attribute, Next - Current);
}

FGameplayAttributeData UAttributeSetBase::GetAttributeData(FGameplayAttribute InAttribute)
{
	const FGameplayAttributeData* Data = InAttribute.GetGameplayAttributeData(this);
	return Data ? *Data : FGameplayAttributeData();
}

float UAttributeSetBase::GetAttributeValue(FGameplayAttribute InAttribute)
{
	return GetAttributeData(InAttribute).GetCurrentValue();
}

float UAttributeSetBase::GetAttributeBaseValue(FGameplayAttribute InAttribute)
{
	return GetAttributeData(InAttribute).GetBaseValue();
}

void UAttributeSetBase::SetAttributeValue(FGameplayAttribute InAttribute, float InValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	if (!AbilityComp || !AbilityComp->IsOwnerActorAuthoritative() || !InAttribute.IsValid() || !InAttribute.GetGameplayAttributeData(this) || !FMath::IsFinite(InValue))
	{
		return;
	}
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Override, InValue);
}

void UAttributeSetBase::ModifyAttributeValue(FGameplayAttribute InAttribute, float InDeltaValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	if (!AbilityComp || !AbilityComp->IsOwnerActorAuthoritative() || !InAttribute.IsValid() || !InAttribute.GetGameplayAttributeData(this) || !FMath::IsFinite(InDeltaValue))
	{
		return;
	}
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Additive, InDeltaValue);
}

void UAttributeSetBase::MultipleAttributeValue(FGameplayAttribute InAttribute, float InMultipleValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	if (!AbilityComp || !AbilityComp->IsOwnerActorAuthoritative() || !InAttribute.IsValid() || !InAttribute.GetGameplayAttributeData(this) || !FMath::IsFinite(InMultipleValue))
	{
		return;
	}
	AbilityComp->ApplyModToAttributeUnsafe(InAttribute, EGameplayModOp::Multiplicitive, InMultipleValue);
}

TArray<FGameplayAttribute> UAttributeSetBase::GetAllAttributes() const
{
	TArray<FGameplayAttribute> AllAttributes;
	for(TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FStructProperty* Property = CastField<FStructProperty>(*PropertyIt);
		if(Property && Property->Struct == FGameplayAttributeData::StaticStruct())
		{
			AllAttributes.Add(Property);
		}
	}
	return AllAttributes;
}

TArray<FGameplayAttribute> UAttributeSetBase::GetPersistentAttributes() const
{
	TArray<FGameplayAttribute> PersistentAttributes;
	for(TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FStructProperty* Property = CastField<FStructProperty>(*PropertyIt);
		if(Property && Property->Struct == FGameplayAttributeData::StaticStruct() && Property->HasAnyPropertyFlags(CPF_SaveGame))
		{
			PersistentAttributes.Add(Property);
		}
	}
	return PersistentAttributes;
}

AActor* UAttributeSetBase::GetOwnerActor(TSubclassOf<AActor> InClass) const
{
	return GetDeterminesOutputObject(GetOwningActor(), InClass);
}
