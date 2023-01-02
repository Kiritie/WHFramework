// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#define MODULE_INSTANCE_DECLARE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
	static ModuleClass* InstanceEditor; \
public: \
	static ModuleClass* Get(bool bInEditor = false); \
	virtual ~##ModuleClass();

#define MODULE_INSTANCE_IMPLEMENTATION(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass* ModuleClass::InstanceEditor = nullptr; \
ModuleClass* ModuleClass::Get(bool bInEditor) \
{ \
	if(!bInEditor) \
	{ \
		if(!Instance) \
		{ \
			Instance = AMainModule::GetModuleByClass<ModuleClass>(false); \
		} \
		return Instance; \
	} \
	else \
	{ \
		if(!InstanceEditor) \
		{ \
			InstanceEditor = AMainModule::GetModuleByClass<ModuleClass>(true); \
		} \
		return InstanceEditor; \
	} \
} \
ModuleClass::~##ModuleClass() \
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