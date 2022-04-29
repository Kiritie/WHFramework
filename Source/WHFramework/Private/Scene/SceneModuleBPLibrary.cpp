// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Scene/SceneModule.h"

void USceneModuleBPLibrary::AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& OnAsyncLoadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AsyncLoadLevel(InLevelPath, OnAsyncLoadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
	}
}

void USceneModuleBPLibrary::AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AsyncUnloadLevel(InLevelPath, InOnAsyncUnloadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
	}
}

float USceneModuleBPLibrary::GetAsyncLoadLevelProgress(FName InLevelPath)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetAsyncLoadLevelProgress(InLevelPath);
	}
	return 0.f;
}

float USceneModuleBPLibrary::GetAsyncUnloadLevelProgress(FName InLevelPath)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetAsyncUnloadLevelProgress(InLevelPath);
	}
	return 0.f;
}

bool USceneModuleBPLibrary::HasSceneActor(TSubclassOf<AActor> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasSceneActor(InClass, bEnsured);
	}
	return false;
}

bool USceneModuleBPLibrary::HasSceneActorByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasSceneActorByName(InName, bEnsured);
	}
	return false;
}

AActor* USceneModuleBPLibrary::GetSceneActor(TSubclassOf<AActor> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetSceneActor(InClass, bEnsured);
	}
	return nullptr;
}

AActor* USceneModuleBPLibrary::GetSceneActorByName(FName InName, TSubclassOf<AActor> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetSceneActorByName(InName, InClass, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddSceneActor(AActor* InActor)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddSceneActor(InActor);
	}
}

void USceneModuleBPLibrary::AddSceneActorByName(FName InName, AActor* InActor)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddSceneActor(InActor);
	}
}

void USceneModuleBPLibrary::RemoveSceneActor(AActor* InActor)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddSceneActor(InActor);
	}
}

void USceneModuleBPLibrary::RemoveSceneActorByName(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemoveSceneActorByName(InName);
	}
}

void USceneModuleBPLibrary::DestroySceneActor(AActor* InActor)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->DestroySceneActor(InActor);
	}
}

void USceneModuleBPLibrary::DestroySceneActorByName(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->DestroySceneActorByName(InName);
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

void USceneModuleBPLibrary::AddTargetPoint(FName InName, ATargetPoint* InPoint)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddTargetPoint(InName, InPoint);
	}
}

void USceneModuleBPLibrary::RemoveTargetPoint(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemoveTargetPoint(InName);
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

void USceneModuleBPLibrary::AddScenePoint(FName InName, USceneComponent* InSceneComp)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddScenePoint(InName, InSceneComp);
	}
}

void USceneModuleBPLibrary::RemoveScenePoint(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemoveScenePoint(InName);
	}
}

bool USceneModuleBPLibrary::HasPhysicsVolume(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasPhysicsVolume(InClass, bEnsured);
	}
	return false;
}

bool USceneModuleBPLibrary::HasPhysicsVolumeByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasPhysicsVolumeByName(InName, bEnsured);
	}
	return false;
}

APhysicsVolumeBase* USceneModuleBPLibrary::GetPhysicsVolume(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetPhysicsVolume(InClass, bEnsured);
	}
	return nullptr;
}

APhysicsVolumeBase* USceneModuleBPLibrary::GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetPhysicsVolumeByName(InName, InClass, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddPhysicsVolume(InPhysicsVolume);
	}
}

void USceneModuleBPLibrary::AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddPhysicsVolumeByName(InName, InPhysicsVolume);
	}
}

void USceneModuleBPLibrary::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemovePhysicsVolume(InPhysicsVolume);
	}
}

void USceneModuleBPLibrary::RemovePhysicsVolumeByName(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemovePhysicsVolumeByName(InName);
	}
}
