#pragma once

#include "CoreMinimal.h"

//////////////////////////////////////////////////////////////////////////
// MACROS
#define PHASEC(A, B) \
((uint8)A & (uint8)B) != 0

#define NUMWITH(A, B) \
(A & B) != 0

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

// ROB
#define DECLARE_PROPERTY_ROB_GETTER(ClassName, PropertyName, PropertyClassName) \
	PropertyClassName ClassName::*Get##PropertyName(); \
	template <PropertyClassName ClassName::*_##PropertyName> \
	struct ClassName##Rob_##PropertyName { \
		friend PropertyClassName ClassName::*Get##PropertyName() { return _##PropertyName; } \
	}; \
	template struct ClassName##Rob_##PropertyName<&ClassName::PropertyName>;

// INI
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
