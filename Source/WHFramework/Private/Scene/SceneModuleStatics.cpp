// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/SceneModuleStatics.h"

#include "Scene/SceneModule.h"

float USceneModuleStatics::GetSeaLevel()
{
	return USceneModule::Get().GetSeaLevel();
}

void USceneModuleStatics::SetSeaLevel(float InSeaLevel)
{
	USceneModule::Get().SetSeaLevel(InSeaLevel);
}

float USceneModuleStatics::GetAltitude(bool bUnsigned, bool bRefresh)
{
	return USceneModule::Get().GetAltitude(bUnsigned, bRefresh);
}

AMiniMapCapture* USceneModuleStatics::GetMiniMapCapture()
{
	return USceneModule::Get().GetMiniMapCapture();
}

EWorldMiniMapMode USceneModuleStatics::GetMiniMapMode()
{
	return USceneModule::Get().GetMiniMapMode();
}

void USceneModuleStatics::SetMiniMapMode(EWorldMiniMapMode InMiniMapMode)
{
	USceneModule::Get().SetMiniMapMode(InMiniMapMode);
}

FTransform USceneModuleStatics::GetMiniMapPoint()
{
	return USceneModule::Get().GetMiniMapPoint();
}

void USceneModuleStatics::SetMiniMapPoint(const FTransform& InMiniMapPoint)
{
	USceneModule::Get().SetMiniMapPoint(InMiniMapPoint);
}

float USceneModuleStatics::GetMiniMapRange()
{
	return USceneModule::Get().GetMiniMapRange();
}

void USceneModuleStatics::SetMiniMapRange(float InMiniMapRange)
{
	USceneModule::Get().SetMiniMapRange(InMiniMapRange);
}

UTextureRenderTarget2D* USceneModuleStatics::GetMiniMapTexture()
{
	return USceneModule::Get().GetMiniMapTexture();
}

void USceneModuleStatics::SetMiniMapTexture(UTextureRenderTarget2D* InMiniMapTexture)
{
	USceneModule::Get().SetMiniMapTexture(InMiniMapTexture);
}

FWorldMaxMapArea USceneModuleStatics::GetMaxMapArea(const FName InName)
{
	return USceneModule::Get().GetMaxMapArea(InName);
}

FWorldMaxMapArea USceneModuleStatics::GetMaxMapAreaByPoint(const FVector2D& InPoint)
{
	return USceneModule::Get().GetMaxMapAreaByPoint(InPoint);
}

TArray<FWorldMaxMapArea> USceneModuleStatics::GetMaxMapAreas()
{
	return USceneModule::Get().GetMaxMapAreas();
}

void USceneModuleStatics::AddMaxMapArea(const FWorldMaxMapArea& InArea)
{
	USceneModule::Get().AddMaxMapArea(InArea);
}

void USceneModuleStatics::RemoveMaxMapArea(const FName InName)
{
	USceneModule::Get().RemoveMaxMapArea(InName);
}

void USceneModuleStatics::ClearMaxMapArea()
{
	USceneModule::Get().ClearMaxMapArea();
}

bool USceneModuleStatics::HasMaxMapArea(const FName InName)
{
	return USceneModule::Get().HasMaxMapArea(InName);
}

UWorldTimer* USceneModuleStatics::GetWorldTimer(TSubclassOf<UWorldTimer> InClass)
{
	return USceneModule::Get().GetWorldTimer(InClass);
}

UWorldWeather* USceneModuleStatics::GetWorldWeather(TSubclassOf<UWorldWeather> InClass)
{
	return USceneModule::Get().GetWorldWeather(InClass);
}

bool USceneModuleStatics::HasTraceMapping(const FName InName, bool bEnsured)
{
	return USceneModule::Get().HasTraceMapping(InName, bEnsured);
}

FTraceMapping USceneModuleStatics::GetTraceMapping(const FName InName, bool bEnsured)
{
	return USceneModule::Get().GetTraceMapping(InName, bEnsured);
}

void USceneModuleStatics::AddTraceMapping(const FName InName, ECollisionChannel InTraceChannel)
{
	USceneModule::Get().AddTraceMapping(InName, InTraceChannel);
}

void USceneModuleStatics::RemoveTraceMapping(const FName InName)
{
	USceneModule::Get().RemoveTraceMapping(InName);
}

void USceneModuleStatics::AsyncLoadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	USceneModule::Get().AsyncLoadLevel(InLevelPath, InOnLoadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

void USceneModuleStatics::AsyncLoadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	USceneModule::Get().AsyncLoadLevelByObjectPtr(InLevelObjectPtr, InOnLoadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

void USceneModuleStatics::AsyncUnloadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	USceneModule::Get().AsyncUnloadLevel(InLevelPath, InOnUnloadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

void USceneModuleStatics::AsyncUnloadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	USceneModule::Get().AsyncUnloadLevelByObjectPtr(InLevelObjectPtr, InOnUnloadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

float USceneModuleStatics::GetAsyncLoadLevelProgress(const FName InLevelPath)
{
	return USceneModule::Get().GetAsyncLoadLevelProgress(InLevelPath);
}

float USceneModuleStatics::GetAsyncLoadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr)
{
	return USceneModule::Get().GetAsyncLoadLevelProgressByObjectPtr(InLevelObjectPtr);
}

float USceneModuleStatics::GetAsyncUnloadLevelProgress(const FName InLevelPath)
{
	return USceneModule::Get().GetAsyncUnloadLevelProgress(InLevelPath);
}

float USceneModuleStatics::GetAsyncUnloadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr)
{
	return USceneModule::Get().GetAsyncUnloadLevelProgressByObjectPtr(InLevelObjectPtr);
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

bool USceneModuleStatics::HasTargetPointByName(const FName InName, bool bEnsured)
{
	return USceneModule::Get().HasTargetPointByName(InName, bEnsured);
}

ATargetPoint* USceneModuleStatics::GetTargetPointByName(const FName InName, bool bEnsured)
{
	return USceneModule::Get().GetTargetPointByName(InName, bEnsured);
}

void USceneModuleStatics::AddTargetPointByName(const FName InName, ATargetPoint* InPoint)
{
	USceneModule::Get().AddTargetPointByName(InName, InPoint);
}

void USceneModuleStatics::RemoveTargetPointByName(const FName InName)
{
	USceneModule::Get().RemoveTargetPointByName(InName);
}

bool USceneModuleStatics::HasScenePointByName(const FName InName, bool bEnsured)
{
	return USceneModule::Get().HasScenePointByName(InName, bEnsured);
}

USceneComponent* USceneModuleStatics::GetScenePointByName(const FName InName, bool bEnsured)
{
	return USceneModule::Get().GetScenePointByName(InName, bEnsured);
}

void USceneModuleStatics::AddScenePointByName(const FName InName, USceneComponent* InSceneComp)
{
	USceneModule::Get().AddScenePointByName(InName, InSceneComp);
}

void USceneModuleStatics::RemoveScenePointByName(const FName InName)
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

bool USceneModuleStatics::HasPhysicsVolumeByName(const FName InName, bool bEnsured)
{
	return USceneModule::Get().HasPhysicsVolumeByName(InName, bEnsured);
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get().GetPhysicsVolumeByClass(InClass, bEnsured);
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByName(const FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get().GetPhysicsVolumeByName(InName, InClass, bEnsured);
}

void USceneModuleStatics::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get().AddPhysicsVolume(InPhysicsVolume);
}

void USceneModuleStatics::AddPhysicsVolumeByName(const FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get().AddPhysicsVolumeByName(InName, InPhysicsVolume);
}

void USceneModuleStatics::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get().RemovePhysicsVolume(InPhysicsVolume);
}

void USceneModuleStatics::RemovePhysicsVolumeByName(const FName InName)
{
	USceneModule::Get().RemovePhysicsVolumeByName(InName);
}

void USceneModuleStatics::SpawnWorldText(const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange)
{
	USceneModule::Get().SpawnWorldText(InText, InTextColor, InTextStyle, InMapping, InOffsetRange);
}
