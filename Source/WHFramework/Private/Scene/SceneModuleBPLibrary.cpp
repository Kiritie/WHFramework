// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"
#include "Scene/SceneModule.h"

UWorldTimerComponent* USceneModuleBPLibrary::GetWorldTimer()
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetWorldTimer();
	}
	return nullptr;
}

UWorldWeatherComponent* USceneModuleBPLibrary::GetWorldWeather()
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetWorldWeather();
	}
	return nullptr;
}

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

bool USceneModuleBPLibrary::HasSceneActor(FGuid InID, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasSceneActor(InID, bEnsured);
	}
	return false;
}

AActor* USceneModuleBPLibrary::GetSceneActor(FGuid InID, TSubclassOf<AActor> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetSceneActor(InID, InClass, bEnsured);
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

void USceneModuleBPLibrary::RemoveSceneActor(AActor* InActor)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddSceneActor(InActor);
	}
}

bool USceneModuleBPLibrary::HasTargetPointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasTargetPointByName(InName, bEnsured);
	}
	return false;
}

ATargetPoint* USceneModuleBPLibrary::GetTargetPointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetTargetPointByName(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddTargetPointByName(FName InName, ATargetPoint* InPoint)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddTargetPointByName(InName, InPoint);
	}
}

void USceneModuleBPLibrary::RemoveTargetPointByName(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemoveTargetPointByName(InName);
	}
}

bool USceneModuleBPLibrary::HasScenePointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasScenePointByName(InName, bEnsured);
	}
	return false;
}

USceneComponent* USceneModuleBPLibrary::GetScenePointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetScenePointByName(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddScenePointByName(FName InName, USceneComponent* InSceneComp)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->AddScenePointByName(InName, InSceneComp);
	}
}

void USceneModuleBPLibrary::RemoveScenePointByName(FName InName)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->RemoveScenePointByName(InName);
	}
}

bool USceneModuleBPLibrary::HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->HasPhysicsVolumeByClass(InClass, bEnsured);
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

APhysicsVolumeBase* USceneModuleBPLibrary::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		return SceneModule->GetPhysicsVolumeByClass(InClass, bEnsured);
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

void USceneModuleBPLibrary::SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FVector InLocation, USceneComponent* InSceneComp)
{
	if(ASceneModule* SceneModule = AMainModule::GetModuleByClass<ASceneModule>())
	{
		SceneModule->SpawnWorldText(InText, InTextColor, InTextStyle, InLocation, InSceneComp);
	}
}
