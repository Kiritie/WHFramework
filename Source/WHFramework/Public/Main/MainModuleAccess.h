#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Main/MainModuleStatics.h"

#define GENERATED_MAIN_MODULE(ModuleClass) \
public: \
	static ModuleClass& Get(UWorld* InWorld); \
	static ModuleClass* GetPtr(UWorld* InWorld);

#define IMPLEMENTATION_MAIN_MODULE(ModuleClass) \
ModuleClass& ModuleClass::Get(UWorld* InWorld) \
{ \
	ModuleClass* Module = ModuleClass::GetPtr(InWorld); \
	UE_CLOG(!Module, LogTemp, Fatal, TEXT("Required main module %s is unavailable in the specified World"), TEXT(#ModuleClass)); \
	return *Module; \
} \
ModuleClass* ModuleClass::GetPtr(UWorld* InWorld) \
{ \
	if(!::IsValid(InWorld)) return nullptr; \
	ModuleClass* Found = nullptr; \
	for (TActorIterator<ModuleClass> It(InWorld); It; ++It) \
	{ \
		if (!::IsValid(*It)) continue; \
		if (Found) { UE_LOG(LogTemp, Error, TEXT("Multiple main module hosts in World %s"), *GetNameSafe(InWorld)); return nullptr; } \
		Found = *It; \
	} \
	return Found; \
}

#define GENERATED_MODULE(ModuleClass) \
public: \
	static ModuleClass& Get(UWorld* InWorld); \
	static ModuleClass* GetPtr(UWorld* InWorld); \

#define IMPLEMENTATION_MODULE(ModuleClass) \
ModuleClass& ModuleClass::Get(UWorld* InWorld) \
{ \
	ModuleClass* Module = ModuleClass::GetPtr(InWorld); \
	UE_CLOG(!Module, LogTemp, Fatal, TEXT("Required module %s is unavailable in the specified World"), TEXT(#ModuleClass)); \
	return *Module; \
} \
ModuleClass* ModuleClass::GetPtr(UWorld* InWorld) \
{ \
	if(!::IsValid(InWorld)) return nullptr; \
	return Cast<ModuleClass>(UMainModuleStatics::GetModuleByClassInWorld(InWorld, ModuleClass::StaticClass())); \
}
