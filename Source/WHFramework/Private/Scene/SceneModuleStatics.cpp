// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleStatics.h"

#include "Scene/SceneModule.h"

UWorldTimerComponent* USceneModuleStatics::GetWorldTimer()
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetWorldTimer();
	}
	return nullptr;
}

UWorldWeatherComponent* USceneModuleStatics::GetWorldWeather()
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetWorldWeather();
	}
	return nullptr;
}

void USceneModuleStatics::AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& OnAsyncLoadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->AsyncLoadLevel(InLevelPath, OnAsyncLoadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
	}
}

void USceneModuleStatics::AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->AsyncUnloadLevel(InLevelPath, InOnAsyncUnloadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
	}
}

float USceneModuleStatics::GetAsyncLoadLevelProgress(FName InLevelPath)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetAsyncLoadLevelProgress(InLevelPath);
	}
	return 0.f;
}

float USceneModuleStatics::GetAsyncUnloadLevelProgress(FName InLevelPath)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetAsyncUnloadLevelProgress(InLevelPath);
	}
	return 0.f;
}

bool USceneModuleStatics::HasSceneActor(FGuid InID, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->HasSceneActor(InID, bEnsured);
	}
	return false;
}

AActor* USceneModuleStatics::GetSceneActor(FGuid InID, TSubclassOf<AActor> InClass, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetSceneActor(InID, InClass, bEnsured);
	}
	return nullptr;
}

bool USceneModuleStatics::AddSceneActor(AActor* InActor)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->AddSceneActor(InActor);
	}
	return false;
}

bool USceneModuleStatics::RemoveSceneActor(AActor* InActor)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->RemoveSceneActor(InActor);
	}
	return false;
}

bool USceneModuleStatics::HasTargetPointByName(FName InName, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->HasTargetPointByName(InName, bEnsured);
	}
	return false;
}

ATargetPoint* USceneModuleStatics::GetTargetPointByName(FName InName, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetTargetPointByName(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleStatics::AddTargetPointByName(FName InName, ATargetPoint* InPoint)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->AddTargetPointByName(InName, InPoint);
	}
}

void USceneModuleStatics::RemoveTargetPointByName(FName InName)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->RemoveTargetPointByName(InName);
	}
}

bool USceneModuleStatics::HasScenePointByName(FName InName, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->HasScenePointByName(InName, bEnsured);
	}
	return false;
}

USceneComponent* USceneModuleStatics::GetScenePointByName(FName InName, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetScenePointByName(InName, bEnsured);
	}
	return nullptr;
}

void USceneModuleStatics::AddScenePointByName(FName InName, USceneComponent* InSceneComp)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->AddScenePointByName(InName, InSceneComp);
	}
}

void USceneModuleStatics::RemoveScenePointByName(FName InName)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->RemoveScenePointByName(InName);
	}
}

APhysicsVolume* USceneModuleStatics::GetDefaultPhysicsVolume()
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetWorld()->GetDefaultPhysicsVolume();
	}
	return nullptr;
}

bool USceneModuleStatics::HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->HasPhysicsVolumeByClass(InClass, bEnsured);
	}
	return false;
}

bool USceneModuleStatics::HasPhysicsVolumeByName(FName InName, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->HasPhysicsVolumeByName(InName, bEnsured);
	}
	return false;
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetPhysicsVolumeByClass(InClass, bEnsured);
	}
	return nullptr;
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		return SceneModule->GetPhysicsVolumeByName(InName, InClass, bEnsured);
	}
	return nullptr;
}

void USceneModuleStatics::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->AddPhysicsVolume(InPhysicsVolume);
	}
}

void USceneModuleStatics::AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->AddPhysicsVolumeByName(InName, InPhysicsVolume);
	}
}

void USceneModuleStatics::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->RemovePhysicsVolume(InPhysicsVolume);
	}
}

void USceneModuleStatics::RemovePhysicsVolumeByName(FName InName)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->RemovePhysicsVolumeByName(InName);
	}
}

void USceneModuleStatics::SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetBindInfo InBindInfo, FVector InOffsetRange)
{
	if(USceneModule* SceneModule = USceneModule::Get())
	{
		SceneModule->SpawnWorldText(InText, InTextColor, InTextStyle, InBindInfo, InOffsetRange);
	}
}
