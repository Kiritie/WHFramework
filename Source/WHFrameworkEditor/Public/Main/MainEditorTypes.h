// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#define GENERATED_EDITOR_MODULE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
public: \
	static ModuleClass& Get();

#define IMPLEMENTATION_EDITOR_MODULE(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass& ModuleClass::Get() \
{ \
	if(!Instance) \
	{ \
		Instance = new ModuleClass(); \
	} \
	return *Instance; \
}
