// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "WHFrameworkCoreTypes.generated.h"

// 不加编译会报错
USTRUCT()
struct FMyStruct
{
	GENERATED_BODY()

};

//////////////////////////////////////////////////////////////////////////
// Variables
extern  WHFRAMEWORKCORE_API UObject* GWorldContext;

//////////////////////////////////////////////////////////////////////////
// Delegates
DECLARE_DYNAMIC_DELEGATE( FSimpleDynamicDelegate );
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FSimpleDynamicMulticastDelegate );

//////////////////////////////////////////////////////////////////////////
// MACROS
#define PHASEC(A, B) \
((uint8)A & (uint8)B) != 0

#define ENUMWITH(A, B) \
((uint8)A & (uint8)B) != 0

#define DON(Count, Expression) \
for(int32 _Index = 0; _Index < Count; _Index++) \
{ \
	Expression \
}

#define DON_WITHINDEX(Count, Index, Expression) \
for(int32 Index = 0; Index < Count; Index++) \
{ \
	Expression \
}

#define ITER_ARRAY(Array, Item, Expression) \
for(auto& Item : Array) \
{ \
	Expression \
}

#define ITER_ARRAY_WITHINDEX(Array, Index, Item, Expression) \
int32 Index = 0; \
for(auto& Item : Array) \
{ \
	Expression \
	Index++; \
}

#define ITER_MAP(Map, Item, Expression) \
for(auto& Item : Map) \
{ \
	Expression \
}

#define ITER_MAP_WITHINDEX(Map, Index, Item, Expression) \
int32 Index = 0; \
for(auto& Item : Map) \
{ \
	Expression \
	Index++; \
}

#define GENERATED_INSTANCE(InstanceClass) \
protected: \
	static InstanceClass* Instance; \
public: \
	static InstanceClass& Get(); \
	static InstanceClass* GetPtr();

#define IMPLEMENTATION_INSTANCE(InstanceClass) \
InstanceClass* InstanceClass::Instance = nullptr; \
InstanceClass& InstanceClass::Get() \
{ \
	return *InstanceClass::GetPtr(); \
} \
InstanceClass* InstanceClass::GetPtr() \
{ \
	if(!Instance) \
	{ \
		Instance = new InstanceClass(); \
	} \
	return Instance; \
}

#define GET_STRING_CONFIG(IniFilename, IniSection, IniKey, OutValue, DefaultValue) \
	FString SettingValue = DefaultValue; \
	bool bSuccess = GConfig->GetString(TEXT(#IniSection), TEXT(#IniKey), SettingValue, IniFilename); \
	OutValue = SettingValue; \
	return bSuccess;
	
#define GET_BOOL_CONFIG(IniFilename, IniSection, IniKey, OutValue, DefaultValue) \
	bool SettingValue = DefaultValue; \
	bool bSuccess = GConfig->GetBool(TEXT(#IniSection), TEXT(#IniKey), SettingValue, IniFilename); \
	OutValue = SettingValue; \
	return bSuccess;
	
#define GET_INT_CONFIG(IniFilename, IniSection, IniKey, OutValue, DefaultValue) \
	int32 SettingValue = DefaultValue; \
	bool bSuccess = GConfig->GetInt(TEXT(#IniSection), TEXT(#IniKey), SettingValue, IniFilename); \
	OutValue = SettingValue; \
	return bSuccess;
	
#define GET_ENUM_CONFIG(IniFilename, IniSection, IniKey, OutValue, DefaultValue, ValueOptions) \
	int32 SettingValue = DefaultValue; \
	bool bSuccess = GConfig->GetInt(TEXT(#IniSection), TEXT(#IniKey), SettingValue, IniFilename); \
	OutValue = FEnumParameterValue(ValueOptions, SettingValue); \
	return bSuccess;

#define SET_STRING_CONFIG(IniFilename, IniSection, IniKey, InValue) \
	GConfig->SetString(TEXT(#IniSection), TEXT(#IniKey), InValue, IniFilename); \
	GConfig->Flush(false, IniFilename)

#define SET_BOOL_CONFIG(IniFilename, IniSection, IniKey, InValue) \
	GConfig->SetBool(TEXT(#IniSection), TEXT(#IniKey), InValue, IniFilename); \
	GConfig->Flush(false, IniFilename)

#define SET_INT_CONFIG(IniFilename, IniSection, IniKey, InValue) \
	GConfig->SetInt(TEXT(#IniSection), TEXT(#IniKey), InValue, IniFilename); \
	GConfig->Flush(false, IniFilename)

#define SET_ENUM_CONFIG(IniFilename, IniSection, IniKey, InValue) \
	GConfig->SetInt(TEXT(#IniSection), TEXT(#IniKey), InValue, IniFilename); \
	GConfig->Flush(false, IniFilename)

/*
 * FUniqueType
 */
struct WHFRAMEWORKCORE_API FUniqueType
{
public:
	using FUniqueID = uint32;

	FUniqueType(const FUniqueType* Parent = nullptr)
		: ID(++NextUniqueID)
		, ParentType(Parent)
	{
		
	}

	bool operator==(const FUniqueType& Other) const
	{
		return ID == Other.ID || (ParentType && *ParentType == Other);
	}

	bool IsA(const FUniqueType& Other) const
	{
		return (ID == Other.ID) || (ParentType && ParentType->IsA(Other));
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("UniqueType_%d"), ID);
	}
	
	friend uint32 GetTypeHash(const FUniqueType& Type)
	{
		return GetTypeHash(Type.ID);
	}

private:
	static FUniqueID NextUniqueID;
	FUniqueID ID;
	const FUniqueType* ParentType;
};

/*
 * FUniqueClass
 */
class WHFRAMEWORKCORE_API FUniqueClass
{
public:
	FUniqueClass(FUniqueType InType)
		: ClassType(InType)
	{
	}

	virtual ~FUniqueClass() {}

	static const FUniqueType Type;

public:
	template <typename T>
	T* CastTo()
	{
		return ClassType.IsA(T::Type) ? StaticCast<T*>(this) : nullptr;
	}
	template <typename T>
	const T* CastTo() const
	{
		return ClassType.IsA(T::Type) ? StaticCast<const T*>(this) : nullptr;
	}
	template <typename T>
	bool IsA() const
	{
		return ClassType.IsA(T::Type);
	}

protected:
	FUniqueType ClassType;

public:
	FUniqueType GetType() const { return ClassType; }
};

/*
 * FSaveDataArchive
 */
struct WHFRAMEWORKCORE_API FSaveDataArchive : public FObjectAndNameAsStringProxyArchive
{
	FSaveDataArchive(FArchive& InInnerArchive) : FObjectAndNameAsStringProxyArchive(InInnerArchive, false)
	{
		ArIsSaveGame = true;
	}
};

//////////////////////////////////////////////////////////////////////////
// Global Functions
template<typename T>
extern void RecursiveSetItems(const T* Value, TFunction<TSet<T*>(const T*)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveSetItems(Iter, Func);
	}
}
template<typename T>
extern void RecursiveSetItems(const T& Value, TFunction<TSet<T>(const T&)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveSetItems(Iter, Func);
	}
}

template<typename T>
extern void RecursiveArrayItems(const T* Value, TFunction<TArray<T*>(const T*)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveArrayItems(Iter, Func);
	}
}
template<typename T>
extern void RecursiveArrayItems(const T& Value, TFunction<TArray<T>(const T&)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveArrayItems(Iter, Func);
	}
}

template<typename TOut, typename TIn>
extern TArray<TOut> CastArrayItems(const TArray<TIn>& InArray)
{
	TArray<TOut> OutArray;
	for(auto Iter : InArray)
	{
		OutArray.Add(Cast<TOut>(Iter));
	}
	return OutArray;
}

template<typename TOut, typename TIn>
extern TArray<TSharedPtr<TOut>> CastSharedPtrArrayItems(const TArray<TSharedPtr<TIn>>& InArray)
{
	TArray<TSharedPtr<TOut>> OutArray;
	for(auto Iter : InArray)
	{
		OutArray.Add(StaticCastSharedPtr<TOut>(Iter));
	}
	return OutArray;
}
