// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleBPLibrary.h"

#include "Scene/SceneModule.h"

UWorldTimerComponent* USceneModuleBPLibrary::GetWorldTimer()
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetWorldTimer();
	}
	return nullptr;
}

UWorldWeatherComponent* USceneModuleBPLibrary::GetWorldWeather()
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetWorldWeather();
	}
	return nullptr;
}

void USceneModuleBPLibrary::AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& OnAsyncLoadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AsyncLoadLevel(InLevelPath, OnAsyncLoadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
	}
}

void USceneModuleBPLibrary::AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AsyncUnloadLevel(InLevelPath, InOnAsyncUnloadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
	}
}

float USceneModuleBPLibrary::GetAsyncLoadLevelProgress(FName InLevelPath)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetAsyncLoadLevelProgress(InLevelPath);
	}
	return 0.f;
}

float USceneModuleBPLibrary::GetAsyncUnloadLevelProgress(FName InLevelPath)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetAsyncUnloadLevelProgress(InLevelPath);
	}
	return 0.f;
}

bool USceneModuleBPLibrary::HasSceneActor(FGuid InID, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->HasSceneActor(InID, bEnsured);
	}
	return false;
}

AActor* USceneModuleBPLibrary::GetSceneActor(FGuid InID, TSubclassOf<AActor> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetSceneActor(InID, InClass, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddSceneActor(AActor* InActor)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AddSceneActor(InActor);
	}
}

void USceneModuleBPLibrary::RemoveSceneActor(AActor* InActor)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AddSceneActor(InActor);
	}
}

bool USceneModuleBPLibrary::HasTargetPointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->HasTargetPointByName(InName, bEnsured);
	}
	return false;
}

ATargetPoint* USceneModuleBPLibrary::GetTargetPointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetTargetPointByName(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddTargetPointByName(FName InName, ATargetPoint* InPoint)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AddTargetPointByName(InName, InPoint);
	}
}

void USceneModuleBPLibrary::RemoveTargetPointByName(FName InName)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->RemoveTargetPointByName(InName);
	}
}

bool USceneModuleBPLibrary::HasScenePointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->HasScenePointByName(InName, bEnsured);
	}
	return false;
}

USceneComponent* USceneModuleBPLibrary::GetScenePointByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetScenePointByName(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddScenePointByName(FName InName, USceneComponent* InSceneComp)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AddScenePointByName(InName, InSceneComp);
	}
}

void USceneModuleBPLibrary::RemoveScenePointByName(FName InName)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->RemoveScenePointByName(InName);
	}
}

APhysicsVolume* USceneModuleBPLibrary::GetDefaultPhysicsVolume()
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetWorld()->GetDefaultPhysicsVolume();
	}
	return nullptr;
}

bool USceneModuleBPLibrary::HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->HasPhysicsVolumeByClass(InClass, bEnsured);
	}
	return false;
}

bool USceneModuleBPLibrary::HasPhysicsVolumeByName(FName InName, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->HasPhysicsVolumeByName(InName, bEnsured);
	}
	return false;
}

APhysicsVolumeBase* USceneModuleBPLibrary::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetPhysicsVolumeByClass(InClass, bEnsured);
	}
	return nullptr;
}

APhysicsVolumeBase* USceneModuleBPLibrary::GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		return SceneModule->GetPhysicsVolumeByName(InName, InClass, bEnsured);
	}
	return nullptr;
}

void USceneModuleBPLibrary::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AddPhysicsVolume(InPhysicsVolume);
	}
}

void USceneModuleBPLibrary::AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->AddPhysicsVolumeByName(InName, InPhysicsVolume);
	}
}

void USceneModuleBPLibrary::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->RemovePhysicsVolume(InPhysicsVolume);
	}
}

void USceneModuleBPLibrary::RemovePhysicsVolumeByName(FName InName)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->RemovePhysicsVolumeByName(InName);
	}
}

void USceneModuleBPLibrary::SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FVector InLocation, FVector InOffsetRange, AActor* InOwnerActor , USceneComponent* InSceneComp)
{
	if(ASceneModule* SceneModule = ASceneModule::Get())
	{
		SceneModule->SpawnWorldText(InText, InTextColor, InTextStyle, InLocation, InOffsetRange, InOwnerActor, InSceneComp);
	}
}
