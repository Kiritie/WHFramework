// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlackboardValuePreChange, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlackboardValueChanged, FName);

#define BLACKBOARD_VALUE_NAME(PropertyName) \
	const FName NAME_##PropertyName = FName(#PropertyName);

#define BLACKBOARD_VALUE_RESETTER(PropertyName) \
	FORCEINLINE void Reset##PropertyName() \
	{ \
		ResetValue(FName(#PropertyName)); \
	}

//////////////////////////////////////////////////////////////////////////
/// INT
#define BLACKBOARD_VALUE_ACCESSORS_INT(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_INT(PropertyName) \
	BLACKBOARD_VALUE_SETTER_INT(PropertyName)

#define BLACKBOARD_VALUE_GETTER_INT(PropertyName) \
	FORCEINLINE int32 Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsInt(FName(#PropertyName)) : 0; \
	}
#define BLACKBOARD_VALUE_SETTER_INT(PropertyName) \
	FORCEINLINE void Set##PropertyName(int32 InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsInt(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_INT(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Int* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Int>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// FLOAT
#define BLACKBOARD_VALUE_ACCESSORS_FLOAT(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_FLOAT(PropertyName) \
	BLACKBOARD_VALUE_SETTER_FLOAT(PropertyName)

#define BLACKBOARD_VALUE_GETTER_FLOAT(PropertyName) \
	FORCEINLINE float Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsFloat(FName(#PropertyName)) : 0.f; \
	}
#define BLACKBOARD_VALUE_SETTER_FLOAT(PropertyName) \
	FORCEINLINE void Set##PropertyName(float InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsFloat(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_FLOAT(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Float* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Float>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// BOOL
#define BLACKBOARD_VALUE_ACCESSORS_BOOL(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_BOOL(PropertyName) \
	BLACKBOARD_VALUE_SETTER_BOOL(PropertyName)

#define BLACKBOARD_VALUE_GETTER_BOOL(PropertyName) \
	FORCEINLINE bool Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsBool(FName(#PropertyName)) : false; \
	}
#define BLACKBOARD_VALUE_SETTER_BOOL(PropertyName) \
	FORCEINLINE void Set##PropertyName(bool InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsBool(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_BOOL(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Bool* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Bool>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// STRING
#define BLACKBOARD_VALUE_ACCESSORS_STRING(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_STRING(PropertyName) \
	BLACKBOARD_VALUE_SETTER_STRING(PropertyName)

#define BLACKBOARD_VALUE_GETTER_STRING(PropertyName) \
	FORCEINLINE FString Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsString(FName(#PropertyName)) : FString(); \
	}
#define BLACKBOARD_VALUE_SETTER_STRING(PropertyName) \
	FORCEINLINE void Set##PropertyName(FString InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsString(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_STRING(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_String* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_String>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// NAME
#define BLACKBOARD_VALUE_ACCESSORS_NAME(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_NAME(PropertyName) \
	BLACKBOARD_VALUE_SETTER_NAME(PropertyName)

#define BLACKBOARD_VALUE_GETTER_NAME(PropertyName) \
	FORCEINLINE FName Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsName(FName(#PropertyName)) : NAME_None; \
	}
#define BLACKBOARD_VALUE_SETTER_NAME(PropertyName) \
	FORCEINLINE void Set##PropertyName(FName InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsName(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_NAME(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Name* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Name>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// VECTOR
#define BLACKBOARD_VALUE_ACCESSORS_VECTOR(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_VECTOR(PropertyName) \
	BLACKBOARD_VALUE_SETTER_VECTOR(PropertyName)

#define BLACKBOARD_VALUE_GETTER_VECTOR(PropertyName) \
	FORCEINLINE FVector Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsVector(FName(#PropertyName)) : FVector::ZeroVector; \
	}
#define BLACKBOARD_VALUE_SETTER_VECTOR(PropertyName) \
	FORCEINLINE void Set##PropertyName(FVector InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsVector(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_VECTOR(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Vector* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Vector>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// ROTATOR
#define BLACKBOARD_VALUE_ACCESSORS_ROTATOR(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_ROTATOR(PropertyName) \
	BLACKBOARD_VALUE_SETTER_ROTATOR(PropertyName)

#define BLACKBOARD_VALUE_GETTER_ROTATOR(PropertyName) \
	FORCEINLINE FRotator Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsRotator(FName(#PropertyName)) : FRotator::ZeroRotator; \
	}
#define BLACKBOARD_VALUE_SETTER_ROTATOR(PropertyName) \
	FORCEINLINE void Set##PropertyName(FRotator InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsRotator(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_ROTATOR(PropertyName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Rotator* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Rotator>(this); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// OBJECT
#define BLACKBOARD_VALUE_ACCESSORS_OBJECT(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_OBJECT(PropertyName) \
	BLACKBOARD_VALUE_SETTER_OBJECT(PropertyName)

#define BLACKBOARD_VALUE_GETTER_OBJECT(PropertyName) \
	template<class T> \
	FORCEINLINE T* Get##PropertyName() const \
	{ \
		return Cast<T>(Get##PropertyName()); \
	} \
	FORCEINLINE UObject* Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsObject(FName(#PropertyName)) : nullptr; \
	}
#define BLACKBOARD_VALUE_SETTER_OBJECT(PropertyName) \
	FORCEINLINE void Set##PropertyName(UObject* InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsObject(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_OBJECT(PropertyName, ClassName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Object* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Object>(this); \
	PropertyName##EntryKeyType->BaseClass = ClassName::StaticClass(); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// CLASS
#define BLACKBOARD_VALUE_ACCESSORS_CLASS(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_CLASS(PropertyName) \
	BLACKBOARD_VALUE_SETTER_CLASS(PropertyName)

#define BLACKBOARD_VALUE_GETTER_CLASS(PropertyName) \
	FORCEINLINE UClass* Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsClass(FName(#PropertyName)) : nullptr; \
	}
#define BLACKBOARD_VALUE_SETTER_CLASS(PropertyName) \
	FORCEINLINE void Set##PropertyName(UClass* InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsClass(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_CLASS(PropertyName, ClassName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Class* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Class>(this); \
	PropertyName##EntryKeyType->BaseClass = ClassName::StaticClass(); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);

//////////////////////////////////////////////////////////////////////////
/// ENUM
#define BLACKBOARD_VALUE_ACCESSORS_ENUM(PropertyName) \
	BLACKBOARD_VALUE_NAME(PropertyName) \
	BLACKBOARD_VALUE_RESETTER(PropertyName) \
	BLACKBOARD_VALUE_GETTER_ENUM(PropertyName) \
	BLACKBOARD_VALUE_SETTER_ENUM(PropertyName)

#define BLACKBOARD_VALUE_GETTER_ENUM(PropertyName) \
	FORCEINLINE uint8 Get##PropertyName() const \
	{ \
		return Component ? Component->GetValueAsEnum(FName(#PropertyName)) : 0; \
	}
#define BLACKBOARD_VALUE_SETTER_ENUM(PropertyName) \
	FORCEINLINE void Set##PropertyName(uint8 InValue) \
	{ \
		if (!Component || Get##PropertyName() == InValue) return; \
		UBlackboardComponent* WriteComponent = Component; \
		OnValuePreChange(FName(#PropertyName)); \
		if (Component != WriteComponent) return; \
		Component->SetValueAsEnum(FName(#PropertyName), InValue); \
		OnValueChanged(FName(#PropertyName)); \
	}

#define BLACKBOARD_VALUE_GENERATE_ENUM(PropertyName, EnumTypeName) \
	FBlackboardEntry PropertyName##Entry; \
	PropertyName##Entry.EntryName = FName(#PropertyName); \
	UBlackboardKeyType_Enum* PropertyName##EntryKeyType = NewObject<UBlackboardKeyType_Enum>(this); \
	PropertyName##EntryKeyType->EnumType = FindObject<UEnum>(nullptr, *FString(#EnumTypeName), true); \
	PropertyName##EntryKeyType->EnumName = TEXT(#EnumTypeName); \
	PropertyName##Entry.KeyType = PropertyName##EntryKeyType; \
	Keys.Add(PropertyName##Entry);
