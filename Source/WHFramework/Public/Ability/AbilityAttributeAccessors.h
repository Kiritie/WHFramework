#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkCoreMacros.h"
#include "WHFrameworkCoreTypes.h"
#include "Common/CommonTypes.h"
#include "GameplayEffect.h"

DECLARE_PROPERTY_ROB_GETTER(FGameplayEffectModifierMagnitude, AttributeBasedMagnitude, FAttributeBasedFloat)

DECLARE_PROPERTY_ROB_GETTER(FGameplayEffectModifierMagnitude, CustomMagnitude, FCustomCalculationBasedFloat)

#define GET_GAMEPLAYATTRIBUTE(ClassName, PropertyName) \
	FGameplayAttribute(GET_MEMBER_PROPERTY(ClassName, PropertyName))

#define GAMEPLAYATTRIBUTE_VALUE_BASE_GETTER(PropertyName) \
	FORCEINLINE float GetBase##PropertyName() const \
	{ \
		return PropertyName.GetBaseValue(); \
	}

#define GAMEPLAYATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_BASE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	ATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_GETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_SETTER(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_MODIFY(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_MULTIPLE(ClassName, PropertyName) \
	ATTRIBUTE_VALUE_INITTER(ClassName, PropertyName)

#define ATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	FORCEINLINE FGameplayAttribute Get##PropertyName##Attribute() const \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		return _AttributeSet->Get##PropertyName##Attribute(); \
	}

#define ATTRIBUTE_VALUE_GETTER(ClassName, PropertyName) \
	FORCEINLINE float Get##PropertyName() const \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		return _AttributeSet->Get##PropertyName(); \
	}\
	FORCEINLINE float GetBase##PropertyName() const \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		return _AttributeSet->GetBase##PropertyName(); \
	}

#define ATTRIBUTE_VALUE_SETTER(ClassName, PropertyName) \
	FORCEINLINE void Set##PropertyName(float InValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->SetAttributeValue(_AttributeSet->Get##PropertyName##Attribute(), InValue); \
	}

#define ATTRIBUTE_VALUE_MODIFY(ClassName, PropertyName) \
	FORCEINLINE void Modify##PropertyName(float InDeltaValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->ModifyAttributeValue(_AttributeSet->Get##PropertyName##Attribute(), InDeltaValue); \
	}

#define ATTRIBUTE_VALUE_MULTIPLE(ClassName, PropertyName) \
	FORCEINLINE void Multiple##PropertyName(float InMultipleValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->MultipleAttributeValue(_AttributeSet->Get##PropertyName##Attribute(), InMultipleValue); \
	}

#define ATTRIBUTE_VALUE_INITTER(ClassName, PropertyName) \
	FORCEINLINE void Init##PropertyName(float InValue) \
	{ \
		ClassName* _AttributeSet = Cast<ClassName>(GetAttributeSet()); \
		_AttributeSet->Init##PropertyName(InValue); \
	}

#define ATTRIBUTE_DELTAVALUE_CLAMP(UserObject, PropertyName, DeltaValue) \
	DeltaValue > 0.f ? FMath::Min(DeltaValue, UserObject->GetMax##PropertyName() - UserObject->Get##PropertyName()) : FMath::Max(DeltaValue, -UserObject->Get##PropertyName())
