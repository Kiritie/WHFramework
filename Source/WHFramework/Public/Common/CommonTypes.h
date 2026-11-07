// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "WHFrameworkCoreTypes.h"

//////////////////////////////////////////////////////////////////////////
// Variables
extern WHFRAMEWORK_API bool GIsPlaying;
extern WHFRAMEWORK_API bool GIsSimulating;

/**
 * 交互对象类型
 */
#define GET_CLASS_NAME(ClassName) #ClassName

//////////////////////////////////////////////////////////////////////////
// Property
#define GET_MEMBER_PROPERTY(ClassName, PropertyName) \
	FindFieldChecked<FProperty>(ClassName::StaticClass(), GET_MEMBER_NAME_CHECKED(ClassName, PropertyName))

//////////////////////////////////////////////////////////////////////////
// Functions
template<class T = UObject>
extern T* GetDeterminesOutputObject(T* Value, UClass* Class)
{
	if(!Value) return nullptr;
	if(!Class || Value->IsA(Class))
	{
		return Value;
	}
	return nullptr;
}

#define GET_FUNCTION_NAME_THISCLASS(FunctionName) \
	GET_FUNCTION_NAME_CHECKED(ThisClass, FunctionName)

//////////////////////////////////////////////////////////////////////////
// Tags
namespace GameplayTags
{
	WHFRAMEWORK_API	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);

}
