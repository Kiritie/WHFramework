// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/UObjectGlobals.h"
#include "Main/MainModuleStatics.h"
#include "Debug/DebugModuleTypes.h"

#include "MainModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EModuleState : uint8
{
	None,
	Running,
	Paused,
	Terminated
};

#define GENERATED_MAIN_MODULE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
	static ModuleClass* InstanceEditor; \
public: \
	static ModuleClass* Get(bool bInEditor = false);

#define GENERATED_MODULE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
	static ModuleClass* InstanceEditor; \
public: \
	static ModuleClass& Get(bool bInEditor = false); \

#define TERMINATION_MAIN_MODULE(ModuleClass) \
	if(ModuleClass::Instance == this) \
	{ \
		ModuleClass::Instance = nullptr; \
	} \
	if(ModuleClass::InstanceEditor == this) \
	{ \
		ModuleClass::InstanceEditor = nullptr; \
	} \

#define TERMINATION_MODULE(ModuleClass) \
	if(ModuleClass::Instance == this) \
	{ \
		ModuleClass::Instance = nullptr; \
	} \
	if(ModuleClass::InstanceEditor == this) \
	{ \
		ModuleClass::InstanceEditor = nullptr; \
	} \

#define IMPLEMENTATION_MAIN_MODULE(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass* ModuleClass::InstanceEditor = nullptr; \
ModuleClass* ModuleClass::Get(bool bInEditor) \
{ \
	if(!bInEditor) \
	{ \
		if(!Instance) \
		{ \
			Instance = UCommonStatics::GetObjectInExistedWorld<ModuleClass>([](const UWorld* World) { \
													return UGameplayStatics::GetActorOfClass(World, ModuleClass::StaticClass()); \
												}, false); \
		} \
		ensureEditorMsgf(Instance, TEXT("Failed to get main module"), EDC_Default, EDV_Error); \
		return Instance; \
	} \
	else \
	{ \
		if(!InstanceEditor) \
		{ \
			InstanceEditor = UCommonStatics::GetObjectInExistedWorld<ModuleClass>([](const UWorld* World) { \
                             						return UGameplayStatics::GetActorOfClass(World, ModuleClass::StaticClass()); \
                             					}, true); \
		} \
		ensureEditorMsgf(InstanceEditor, TEXT("Failed to get main module"), EDC_Default, EDV_Error); \
		return InstanceEditor; \
	} \
}

#define IMPLEMENTATION_MODULE(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass* ModuleClass::InstanceEditor = nullptr; \
ModuleClass& ModuleClass::Get(bool bInEditor) \
{ \
	if(!bInEditor) \
	{ \
		if(!Instance) \
		{ \
			Instance = Cast<ModuleClass>(UMainModuleStatics::GetModuleByClass(ModuleClass::StaticClass(), false)); \
		} \
		ensureEditorMsgf(Instance, FString::Printf(TEXT("Failed to get module, module name: %s"), *ModuleClass::StaticClass()->GetDefaultObject<UModuleBase>()->GetModuleName().ToString()), EDC_Default, EDV_Error); \
		return Instance ? *Instance : *NewObject<ModuleClass>(); \
	} \
	else \
	{ \
		if(!InstanceEditor) \
		{ \
			InstanceEditor = Cast<ModuleClass>(UMainModuleStatics::GetModuleByClass(ModuleClass::StaticClass(), true)); \
		} \
		ensureEditorMsgf(Instance, FString::Printf(TEXT("Failed to get module, module name: %s"), *ModuleClass::StaticClass()->GetDefaultObject<UModuleBase>()->GetModuleName().ToString()), EDC_Default, EDV_Error); \
		return InstanceEditor ? *InstanceEditor : *NewObject<ModuleClass>(); \
	} \
}

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FModuleListItemStates
{
	GENERATED_BODY()

public:
	FORCEINLINE FModuleListItemStates()
	{
	}
};
