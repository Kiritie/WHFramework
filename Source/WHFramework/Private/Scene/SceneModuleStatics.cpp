

#include "Scene/SceneModuleStatics.h"
#include "Widget/WorldWidgetTypes.h"
#include "Scene/SceneMapTypes.h"
#include "Scene/SceneAreaTypes.h"
#include "Scene/SceneMarkerTypes.h"
#include "Scene/SceneStreamingTypes.h"

#include "Scene/SceneModule.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"
#include "Misc/PackageName.h"

namespace
{
	UWorld* ResolveSceneWorld(const UObject* Context)
	{
		return GEngine && Context ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
	}
}

float USceneModuleStatics::GetSeaLevel(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetSeaLevel();
}

void USceneModuleStatics::SetSeaLevel(const UObject* WorldContextObject, float InSeaLevel)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetSeaLevel(InSeaLevel);
}

float USceneModuleStatics::GetAltitude(const UObject* WorldContextObject, APlayerController* InPlayer, bool bUnsigned)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetAltitude(InPlayer, bUnsigned);
}

AMiniMapCapture* USceneModuleStatics::GetMiniMapCapture(const UObject* WorldContextObject, APlayerController* InPlayer)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMiniMapCapture(InPlayer);
}

EWorldMiniMapMode USceneModuleStatics::GetMiniMapMode(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMiniMapMode();
}

void USceneModuleStatics::SetMiniMapMode(const UObject* WorldContextObject, EWorldMiniMapMode InMiniMapMode)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetMiniMapMode(InMiniMapMode);
}

FTransform USceneModuleStatics::GetMiniMapPoint(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMiniMapPoint();
}

void USceneModuleStatics::SetMiniMapPoint(const UObject* WorldContextObject, const FTransform& InMiniMapPoint)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetMiniMapPoint(InMiniMapPoint);
}

float USceneModuleStatics::GetMiniMapRange(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMiniMapRange();
}

void USceneModuleStatics::SetMiniMapRange(const UObject* WorldContextObject, float InMiniMapRange)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetMiniMapRange(InMiniMapRange);
}

UTextureRenderTarget2D* USceneModuleStatics::GetMiniMapTexture(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMiniMapTexture();
}

void USceneModuleStatics::SetMiniMapTexture(const UObject* WorldContextObject, UTextureRenderTarget2D* InMiniMapTexture)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetMiniMapTexture(InMiniMapTexture);
}

FSceneArea USceneModuleStatics::GetSceneArea(const UObject* WorldContextObject, const FName InName)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetSceneArea(InName);
}

FVector2D USceneModuleStatics::GetWorldMapCenter(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetWorldMapCenter();
}

void USceneModuleStatics::SetWorldMapCenter(const UObject* WorldContextObject, FVector2D InCenter)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetWorldMapCenter(InCenter);
}

float USceneModuleStatics::GetWorldMapRange(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetWorldMapRange();
}

void USceneModuleStatics::SetWorldMapRange(const UObject* WorldContextObject, float InRange)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetWorldMapRange(InRange);
}

FSceneArea USceneModuleStatics::GetSceneAreaByPoint(const UObject* WorldContextObject, const FVector2D& InPoint)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetSceneAreaByPoint(InPoint);
}

TArray<FSceneArea> USceneModuleStatics::GetSceneAreas(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetSceneAreas();
}

void USceneModuleStatics::AddSceneArea(const UObject* WorldContextObject, const FSceneArea& InArea, bool bThreadSafe)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddSceneArea(InArea, bThreadSafe);
}

void USceneModuleStatics::RemoveSceneArea(const UObject* WorldContextObject, const FName InName)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemoveSceneArea(InName);
}

void USceneModuleStatics::ClearSceneArea(const UObject* WorldContextObject)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).ClearSceneArea();
}

FGuid USceneModuleStatics::AddMarker(const UObject* WorldContextObject, const FSceneMarker& InMarker)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddMarker(InMarker);
}

bool USceneModuleStatics::UpdateMarker(const UObject* WorldContextObject, const FSceneMarker& InMarker)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).UpdateMarker(InMarker);
}

bool USceneModuleStatics::RemoveMarker(const UObject* WorldContextObject, FGuid InMarkerID)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemoveMarker(InMarkerID);
}

void USceneModuleStatics::ClearMarkers(const UObject* WorldContextObject, bool bIncludePersistent)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).ClearMarkers(bIncludePersistent);
}

FSceneMarker USceneModuleStatics::GetMarker(const UObject* WorldContextObject, FGuid InMarkerID)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMarker(InMarkerID);
}

TArray<FSceneMarkerView> USceneModuleStatics::GetMarkerViews(const UObject* WorldContextObject, FGuid InPlayerID, ESceneMarkerChannel InChannel, FVector InViewLocation, float InViewYaw)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetMarkerViews(InPlayerID, InChannel, InViewLocation, InViewYaw);
}

bool USceneModuleStatics::SetTrackedMarker(const UObject* WorldContextObject, FGuid InPlayerID, FGuid InMarkerID)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SetTrackedMarker(InPlayerID, InMarkerID);
}

FGuid USceneModuleStatics::GetTrackedMarker(const UObject* WorldContextObject, FGuid InPlayerID)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetTrackedMarker(InPlayerID);
}

bool USceneModuleStatics::ProjectMarkerToMap(const FSceneMarkerView& InMarker, FVector2D InCenter, float InRange, FVector2D InSize, float InYaw, bool bClamp, FVector2D& OutPosition)
{
	if(InRange <= UE_SMALL_NUMBER || InSize.X <= 0.f || InSize.Y <= 0.f) return false;
	FSceneMapView View;
	View.Center = InCenter;
	View.Range = InRange;
	View.Yaw = InYaw;
	OutPosition = View.WorldToPanel(FVector2D(InMarker.Location), InSize);
	const bool bInside = OutPosition.X >= 0.f && OutPosition.X <= InSize.X && OutPosition.Y >= 0.f && OutPosition.Y <= InSize.Y;
	if(!bInside && !bClamp) return false;
	if(bClamp)
	{
		OutPosition.X = FMath::Clamp(OutPosition.X, 0.f, InSize.X);
		OutPosition.Y = FMath::Clamp(OutPosition.Y, 0.f, InSize.Y);
	}
	return true;
}

bool USceneModuleStatics::ProjectMarkerToCompass(const FSceneMarkerView& InMarker, float InFieldOfView, float InWidth, bool bClamp, float& OutPosition)
{
	if(InFieldOfView <= UE_SMALL_NUMBER || InWidth <= 0.f) return false;
	const float Normalized = InMarker.Bearing / InFieldOfView;
	if(FMath::Abs(Normalized) > 0.5f && !bClamp) return false;
	OutPosition = (FMath::Clamp(Normalized, -0.5f, 0.5f) + 0.5f) * InWidth;
	return true;
}

bool USceneModuleStatics::ProjectMarkerToScreen(APlayerController* InPlayer, const FSceneMarkerView& InMarker, bool bPlayerViewportRelative, FVector2D& OutPosition)
{
	return InPlayer && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(InPlayer, InMarker.Location, OutPosition, bPlayerViewportRelative);
}

bool USceneModuleStatics::HasSceneArea(const UObject* WorldContextObject, const FName InName)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasSceneArea(InName);
}

UWorldTimer* USceneModuleStatics::GetWorldTimer(const UObject* WorldContextObject, TSubclassOf<UWorldTimer> InClass)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetWorldTimer(InClass);
}

UWorldWeather* USceneModuleStatics::GetWorldWeather(const UObject* WorldContextObject, TSubclassOf<UWorldWeather> InClass)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetWorldWeather(InClass);
}

bool USceneModuleStatics::HasTraceMapping(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasTraceMapping(InName, bEnsured);
}

FTraceMapping USceneModuleStatics::GetTraceMapping(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetTraceMapping(InName, bEnsured);
}

void USceneModuleStatics::AddTraceMapping(const UObject* WorldContextObject, const FName InName, ECollisionChannel InTraceChannel)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddTraceMapping(InName, InTraceChannel);
}

void USceneModuleStatics::RemoveTraceMapping(const UObject* WorldContextObject, const FName InName)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemoveTraceMapping(InName);
}

void USceneModuleStatics::AsyncLoadLevel(const UObject* WorldContextObject, const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if (USceneModule* Module = USceneModule::GetPtr(ResolveSceneWorld(WorldContextObject)))
		Module->AsyncLoadLevel(InLevelPath, InOnLoadFinished, InFinishDelayTime, bCreateLoadingWidget);
	else InOnLoadFinished.ExecuteIfBound(InLevelPath, ESceneLevelLoadResult::Failed);
}

void USceneModuleStatics::AsyncLoadLevelByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if (USceneModule* Module = USceneModule::GetPtr(ResolveSceneWorld(WorldContextObject)))
		Module->AsyncLoadLevelByObjectPtr(InLevelObjectPtr, InOnLoadFinished, InFinishDelayTime, bCreateLoadingWidget);
	else InOnLoadFinished.ExecuteIfBound(FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString())), ESceneLevelLoadResult::Failed);
}

void USceneModuleStatics::AsyncUnloadLevel(const UObject* WorldContextObject, const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if (USceneModule* Module = USceneModule::GetPtr(ResolveSceneWorld(WorldContextObject)))
		Module->AsyncUnloadLevel(InLevelPath, InOnUnloadFinished, InFinishDelayTime, bCreateLoadingWidget);
	else InOnUnloadFinished.ExecuteIfBound(InLevelPath, ESceneLevelLoadResult::Failed);
}

void USceneModuleStatics::AsyncUnloadLevelByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	if (USceneModule* Module = USceneModule::GetPtr(ResolveSceneWorld(WorldContextObject)))
		Module->AsyncUnloadLevelByObjectPtr(InLevelObjectPtr, InOnUnloadFinished, InFinishDelayTime, bCreateLoadingWidget);
	else InOnUnloadFinished.ExecuteIfBound(FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString())), ESceneLevelLoadResult::Failed);
}

float USceneModuleStatics::GetAsyncLoadLevelProgress(const UObject* WorldContextObject, const FName InLevelPath)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetAsyncLoadLevelProgress(InLevelPath);
}

float USceneModuleStatics::GetAsyncLoadLevelProgressByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetAsyncLoadLevelProgressByObjectPtr(InLevelObjectPtr);
}

float USceneModuleStatics::GetAsyncUnloadLevelProgress(const UObject* WorldContextObject, const FName InLevelPath)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetAsyncUnloadLevelProgress(InLevelPath);
}

float USceneModuleStatics::GetAsyncUnloadLevelProgressByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetAsyncUnloadLevelProgressByObjectPtr(InLevelObjectPtr);
}

bool USceneModuleStatics::HasSceneActor(const UObject* WorldContextObject, const FString& InID, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasSceneActor(InID, bEnsured);
}

AActor* USceneModuleStatics::GetSceneActor(const UObject* WorldContextObject, const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetSceneActor(InID, InClass, bEnsured);
}

bool USceneModuleStatics::AddSceneActor(const UObject* WorldContextObject, AActor* InActor)
{
	UWorld* World = ResolveSceneWorld(WorldContextObject);
	USceneModule* Module = USceneModule::GetPtr(World);
	return Module && InActor && InActor->GetWorld() == World && Module->AddSceneActor(InActor);
}

bool USceneModuleStatics::RemoveSceneActor(const UObject* WorldContextObject, AActor* InActor)
{
	UWorld* World = ResolveSceneWorld(WorldContextObject);
	USceneModule* Module = USceneModule::GetPtr(World);
	return Module && InActor && InActor->GetWorld() == World && Module->RemoveSceneActor(InActor);
}

bool USceneModuleStatics::HasTargetPointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasTargetPointByName(InName, bEnsured);
}

ATargetPoint* USceneModuleStatics::GetTargetPointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetTargetPointByName(InName, bEnsured);
}

void USceneModuleStatics::AddTargetPointByName(const UObject* WorldContextObject, const FName InName, ATargetPoint* InPoint)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddTargetPointByName(InName, InPoint);
}

void USceneModuleStatics::RemoveTargetPointByName(const UObject* WorldContextObject, const FName InName)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemoveTargetPointByName(InName);
}

bool USceneModuleStatics::HasScenePointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasScenePointByName(InName, bEnsured);
}

USceneComponent* USceneModuleStatics::GetScenePointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetScenePointByName(InName, bEnsured);
}

void USceneModuleStatics::AddScenePointByName(const UObject* WorldContextObject, const FName InName, USceneComponent* InSceneComp)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddScenePointByName(InName, InSceneComp);
}

void USceneModuleStatics::RemoveScenePointByName(const UObject* WorldContextObject, const FName InName)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemoveScenePointByName(InName);
}

APhysicsVolume* USceneModuleStatics::GetDefaultPhysicsVolume(const UObject* WorldContextObject)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetWorld()->GetDefaultPhysicsVolume();
}

bool USceneModuleStatics::HasPhysicsVolumeByClass(const UObject* WorldContextObject, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasPhysicsVolumeByClass(InClass, bEnsured);
}

bool USceneModuleStatics::HasPhysicsVolumeByName(const UObject* WorldContextObject, const FName InName, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).HasPhysicsVolumeByName(InName, bEnsured);
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByClass(const UObject* WorldContextObject, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetPhysicsVolumeByClass(InClass, bEnsured);
}

APhysicsVolumeBase* USceneModuleStatics::GetPhysicsVolumeByName(const UObject* WorldContextObject, const FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured)
{
	return USceneModule::Get(ResolveSceneWorld(WorldContextObject)).GetPhysicsVolumeByName(InName, InClass, bEnsured);
}

void USceneModuleStatics::AddPhysicsVolume(const UObject* WorldContextObject, APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddPhysicsVolume(InPhysicsVolume);
}

void USceneModuleStatics::AddPhysicsVolumeByName(const UObject* WorldContextObject, const FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).AddPhysicsVolumeByName(InName, InPhysicsVolume);
}

void USceneModuleStatics::RemovePhysicsVolume(const UObject* WorldContextObject, APhysicsVolumeBase* InPhysicsVolume)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemovePhysicsVolume(InPhysicsVolume);
}

void USceneModuleStatics::RemovePhysicsVolumeByName(const UObject* WorldContextObject, const FName InName)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).RemovePhysicsVolumeByName(InName);
}

void USceneModuleStatics::SpawnWorldText(const UObject* WorldContextObject, const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange)
{
	USceneModule::Get(ResolveSceneWorld(WorldContextObject)).SpawnWorldText(InText, InTextColor, InTextStyle, InMapping, InOffsetRange);
}
