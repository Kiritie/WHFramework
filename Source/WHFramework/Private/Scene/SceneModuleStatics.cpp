// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleStatics.h"

#include "Scene/SceneModule.h"

float USceneModuleStatics::GetSeaLevel()
{
	return USceneModule::Get().GetSeaLevel();
}

float USceneModuleStatics::GetAltitude(bool bUnsigned)
{
	return USceneModule::Get().GetAltitude(bUnsigned);
}

UWorldTimer* USceneModuleStatics::GetWorldTimer(TSubclassOf<UWorldTimer> InClass)
{
	return USceneModule::Get().GetWorldTimer(InClass);
}

UWorldWeather* USceneModuleStatics::GetWorldWeather(TSubclassOf<UWorldWeather> InClass)
{
	return USceneModule::Get().GetWorldWeather(InClass);
}

void USceneModuleStatics::AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& OnAsyncLoadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	USceneModule::Get().AsyncLoadLevel(InLevelPath, OnAsyncLoadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
}

void USceneModuleStatics::AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	USceneModule::Get().AsyncUnloadLevel(InLevelPath, InOnAsyncUnloadLevelFinished, InFinishDelayTime, bCreateLoadingWidget);
}

float USceneModuleStatics::GetAsyncLoadLevelProgress(FName InLevelPath)
{
	return USceneModule::Get().GetAsyncLoadLevelProgress(InLevelPath);
}

float USceneModuleStatics::GetAsyncUnloadLevelProgress(FName InLevelPath)
{
	return USceneModule::Get().GetAsyncUnloadLevelProgress(InLevelPath);
}

bool USceneModuleStatics::HasSceneActor(const FString& InID, bool bEnsured)
{
	return USceneModule::Get().HasSceneActor(InID, bEnsured);
}

AActor* USceneModuleStatics::GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured)
{
	return USceneModule::Get().GetSceneActor(InID, InClass, bEnsured);
}

bool USceneModuleStatics::AddSceneActor(AActor* InActor)
{
	return USceneModule::Get().AddSceneActor(InActor);
}

bool USceneModuleStatics::RemoveSceneActor(AActor* InActor)
{
	return USceneModule::Get().RemoveSceneActor(InActor);
}

bool USceneModuleStatics::HasTargetPointByName(FName InName, bool bEnsured)
{
	return USceneModule::Get().HasTargetPointByName(InName, bEnsured);
}

ATargetPoint* USceneModuleStatics::GetTargetPointByName(FName InName, bool bEnsured)
{
	return USceneModule::Get().GetTargetPointByName(InName, bEnsured);
}

void USceneModuleStatics::AddTargetPointByName(FName InName, ATargetPoint* InPoint)
{
	USceneModule::Get().AddTargetPointByName(InName, InPoint);
}

void USceneModuleStatics::RemoveTargetPointByName(FName InName)
{
	USceneModule::Get().RemoveTargetPointByName(InName);
}

bool USceneModuleStatics::HasScenePointByName(FName InName, bool bEnsured)
{
	return USceneModule::Get().HasScenePointByName(InName, bEnsured);
}

USceneComponent* USceneModuleStatics::GetScenePointByName(FName InName, bool bEnsured)
{
	return USceneModule::Get().GetScenePointByName(InName, bEnsured);
}

void USceneModuleStatics::AddScenePointByName(FName InName, USceneComponent* InSceneComp)
{
	USceneModule::Get().AddScenePointByName(InName, InSceneComp);
}

void USceneModuleStatics::RemoveScenePointByName(FName InName)
{
	USceneModule::Get().RemoveScenePointByName(InName);
}

APhysicsVolume* USceneModuleStatics::GetDefaultPhysicsVolume()
{
	return USceneModule::Get().GetWorld()->GetDefaultPhysicsVolume();
}

bool USceneModuleStatics::HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get().HasPhysicsVolumeByClass(InClass, bEnsured);
}

bool USceneModuleStatics::HasPhysicsVolumeByName(FName InName, bool bEnsured)
{
	return USceneModule::Get().HasPhysicsVolumeByName(InName, bEnsured);
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get().GetPhysicsVolumeByClass(InClass, bEnsured);
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get().GetPhysicsVolumeByName(InName, InClass, bEnsured);
}

void USceneModuleStatics::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get().AddPhysicsVolume(InPhysicsVolume);
}

void USceneModuleStatics::AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get().AddPhysicsVolumeByName(InName, InPhysicsVolume);
}

void USceneModuleStatics::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get().RemovePhysicsVolume(InPhysicsVolume);
}

void USceneModuleStatics::RemovePhysicsVolumeByName(FName InName)
{
	USceneModule::Get().RemovePhysicsVolumeByName(InName);
}

void USceneModuleStatics::SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetBindInfo InBindInfo, FVector InOffsetRange)
{
	USceneModule::Get().SpawnWorldText(InText, InTextColor, InTextStyle, InBindInfo, InOffsetRange);
}
