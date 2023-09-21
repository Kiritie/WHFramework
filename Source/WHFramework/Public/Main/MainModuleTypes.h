// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/UObjectGlobals.h"
#include "Main/MainModuleBPLibrary.h"

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
	static ModuleClass* Get(bool bInEditor = false); \

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
			Instance = UCommonBPLibrary::GetObjectInExistedWorld<ModuleClass>([](const UWorld* World) { \
													return UGameplayStatics::GetActorOfClass(World, ModuleClass::StaticClass()); \
												}, false); \
		} \
		return Instance; \
	} \
	else \
	{ \
		if(!InstanceEditor) \
		{ \
			InstanceEditor = UCommonBPLibrary::GetObjectInExistedWorld<ModuleClass>([](const UWorld* World) { \
                             						return UGameplayStatics::GetActorOfClass(World, ModuleClass::StaticClass()); \
                             					}, true); \
		} \
		return InstanceEditor; \
	} \
} \

#define IMPLEMENTATION_MODULE(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass* ModuleClass::InstanceEditor = nullptr; \
ModuleClass* ModuleClass::Get(bool bInEditor) \
{ \
	if(!bInEditor) \
	{ \
		if(!Instance) \
		{ \
			Instance = Cast<ModuleClass>(UMainModuleBPLibrary::GetModuleByClass(ModuleClass::StaticClass(), false)); \
		} \
		return Instance; \
	} \
	else \
	{ \
		if(!InstanceEditor) \
		{ \
			InstanceEditor = Cast<ModuleClass>(UMainModuleBPLibrary::GetModuleByClass(ModuleClass::StaticClass(), true)); \
		} \
		return InstanceEditor; \
	} \
} \
