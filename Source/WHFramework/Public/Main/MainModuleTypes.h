// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "UObject/UObjectGlobals.h"

#define GENERATED_MAIN_MODULE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
	static ModuleClass* InstanceEditor; \
public: \
	static ModuleClass* Get(bool bInEditor = false); \
	virtual void OnTermination_Internal() override;

#define GENERATED_MODULE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
	static ModuleClass* InstanceEditor; \
public: \
	static ModuleClass* Get(bool bInEditor = false); \
	virtual void OnTermination_Internal() override;

#define IMPLEMENTATION_MAIN_MODULE(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass* ModuleClass::InstanceEditor = nullptr; \
ModuleClass* ModuleClass::Get(bool bInEditor) \
{ \
	if(!bInEditor) \
	{ \
		if(!Instance) \
		{ \
			Instance = UGlobalBPLibrary::GetObjectInExistedWorld<ModuleClass>([](const UWorld* World) { \
													return UGameplayStatics::GetActorOfClass(World, ModuleClass::StaticClass()); \
												}, false); \
		} \
		return Instance; \
	} \
	else \
	{ \
		if(!InstanceEditor) \
		{ \
			InstanceEditor = UGlobalBPLibrary::GetObjectInExistedWorld<ModuleClass>([](const UWorld* World) { \
                             						return UGameplayStatics::GetActorOfClass(World, ModuleClass::StaticClass()); \
                             					}, true); \
		} \
		return InstanceEditor; \
	} \
} \
void ModuleClass::OnTermination_Internal() \
{ \
	if(Instance == this) \
	{ \
		Instance = nullptr; \
	} \
	if(InstanceEditor == this) \
	{ \
		InstanceEditor = nullptr; \
	} \
}

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
			Instance = Cast<ModuleClass>(UMainModuleBPLibrary::GetModuleByClass(ModuleClass::StaticClass(), true)); \
		} \
		return InstanceEditor; \
	} \
} \
void ModuleClass::OnTermination_Internal() \
{ \
	if(Instance == this) \
	{ \
		Instance = nullptr; \
	} \
	if(InstanceEditor == this) \
	{ \
		InstanceEditor = nullptr; \
	} \
}
