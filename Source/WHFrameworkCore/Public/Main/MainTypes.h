// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#define GENERATED_MAIN_MANAGER(ManagerClass) \
protected: \
	static ManagerClass* Instance; \
public: \
	static ManagerClass& Get(); \
	static ManagerClass* GetPtr();

#define IMPLEMENTATION_MAIN_MANAGER(ManagerClass) \
ManagerClass* ManagerClass::Instance = nullptr; \
ManagerClass& ManagerClass::Get() \
{ \
	ManagerClass* Manager = ManagerClass::GetPtr(); \
	UE_CLOG(!Manager, LogTemp, Fatal, TEXT("Required core manager registry is unavailable")); \
	return *Manager; \
} \
ManagerClass* ManagerClass::GetPtr() \
{ \
	return Instance; \
}

#define GENERATED_MANAGER(ManagerClass) \
public: \
	static ManagerClass& Get(); \
	static ManagerClass* GetPtr(); \
	static void Register(bool bForce = false); \
	static void UnRegister();

#define IMPLEMENTATION_MANAGER(ManagerClass) \
ManagerClass& ManagerClass::Get() \
{ \
	ManagerClass* Manager = ManagerClass::GetPtr(); \
	UE_CLOG(!Manager, LogTemp, Fatal, TEXT("Required core manager %s is unavailable"), TEXT(#ManagerClass)); \
	return *Manager; \
} \
ManagerClass* ManagerClass::GetPtr() \
{ \
	return FMainManager::GetManager<ManagerClass>(); \
} \
void ManagerClass::Register(bool bForce) \
{ \
	FMainManager::RegisterManager<ManagerClass>(bForce); \
} \
void ManagerClass::UnRegister() \
{ \
	FMainManager::UnRegisterManager<ManagerClass>(); \
}
