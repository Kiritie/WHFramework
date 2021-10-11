// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Scene/SceneModule.h"

bool USceneModuleBPLibrary::HasSceneObject(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasSceneObject(InName, bEnsured);
	}
	return false;
}

TScriptInterface<ISceneObject> USceneModuleBPLibrary::GetSceneObject(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetSceneObject(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::SetSceneObject(FName InName, TScriptInterface<ISceneObject> InObject)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->SetSceneObject(InName, InObject);
	}
}

bool USceneModuleBPLibrary::HasTargetPoint(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasTargetPoint(InName, bEnsured);
	}
	return false;
}

ATargetPoint* USceneModuleBPLibrary::GetTargetPoint(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetTargetPoint(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::SetTargetPoint(FName InName, ATargetPoint* InPoint)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->SetTargetPoint(InName, InPoint);
	}
}

bool USceneModuleBPLibrary::HasScenePoint(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasScenePoint(InName, bEnsured);
	}
	return false;
}

USceneComponent* USceneModuleBPLibrary::GetScenePoint(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetScenePoint(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::SetScenePoint(FName InName, USceneComponent* InSceneComp)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->SetScenePoint(InName, InSceneComp);
	}
}

bool USceneModuleBPLibrary::HasPhysicsVolume(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasPhysicsVolume(InName, bEnsured);
	}
	return false;
}

APhysicsVolumeBase* USceneModuleBPLibrary::GetPhysicsVolume(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetPhysicsVolume(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::SetPhysicsVolume(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->SetPhysicsVolume(InName, InPhysicsVolume);
	}
}
