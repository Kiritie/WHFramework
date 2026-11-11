
#pragma once

#include "Widget/WorldWidgetTypes.h"
#include "Scene/SceneMapTypes.h"
#include "Scene/SceneAreaTypes.h"
#include "Scene/SceneMarkerTypes.h"
#include "Scene/SceneStreamingTypes.h"
#include "SceneModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Widget/WidgetModuleTypes.h"

#include "SceneModuleStatics.generated.h"

class AMiniMapCapture;
class UWorldWeather;
class UWorldTimer;
class APhysicsVolumeBase;
class APhysicsVolume;
class APlayerController;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USceneModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetSeaLevel(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetSeaLevel(const UObject* WorldContextObject, float InSeaLevel);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetAltitude(const UObject* WorldContextObject, APlayerController* InPlayer, bool bUnsigned = false);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static AMiniMapCapture* GetMiniMapCapture(const UObject* WorldContextObject, APlayerController* InPlayer);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static EWorldMiniMapMode GetMiniMapMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetMiniMapMode(const UObject* WorldContextObject, EWorldMiniMapMode InMiniMapMode);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FTransform GetMiniMapPoint(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetMiniMapPoint(const UObject* WorldContextObject, const FTransform& InMiniMapPoint);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetMiniMapRange(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetMiniMapRange(const UObject* WorldContextObject, float InMiniMapRange);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UTextureRenderTarget2D* GetMiniMapTexture(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetMiniMapTexture(const UObject* WorldContextObject, UTextureRenderTarget2D* InMiniMapTexture);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FVector2D GetWorldMapCenter(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetWorldMapCenter(const UObject* WorldContextObject, FVector2D InCenter);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetWorldMapRange(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SetWorldMapRange(const UObject* WorldContextObject, float InRange);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasSceneArea(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSceneArea GetSceneArea(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FSceneArea GetSceneAreaByPoint(const UObject* WorldContextObject, const FVector2D& InPoint);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSceneArea> GetSceneAreas(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddSceneArea(const UObject* WorldContextObject, const FSceneArea& InArea, bool bThreadSafe = false);
	
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveSceneArea(const UObject* WorldContextObject, const FName InName);
		
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void ClearSceneArea(const UObject* WorldContextObject);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static FGuid AddMarker(const UObject* WorldContextObject, const FSceneMarker& InMarker);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static bool UpdateMarker(const UObject* WorldContextObject, const FSceneMarker& InMarker);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static bool RemoveMarker(const UObject* WorldContextObject, FGuid InMarkerID);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static void ClearMarkers(const UObject* WorldContextObject, bool bIncludePersistent = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static FSceneMarker GetMarker(const UObject* WorldContextObject, FGuid InMarkerID);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSceneMarkerView> GetMarkerViews(const UObject* WorldContextObject, FGuid InPlayerID, ESceneMarkerChannel InChannel, FVector InViewLocation, float InViewYaw = 0.f);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static bool SetTrackedMarker(const UObject* WorldContextObject, FGuid InPlayerID, FGuid InMarkerID);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics|Marker", meta = (WorldContext = "WorldContextObject"))
	static FGuid GetTrackedMarker(const UObject* WorldContextObject, FGuid InPlayerID);

	/** 将世界标记投影到以指定位置为中心的地图面板，范围表示可见世界宽度 */
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics|Marker")
	static bool ProjectMarkerToMap(const FSceneMarkerView& InMarker, FVector2D InCenter, float InRange, FVector2D InSize, float InYaw, bool bClamp, FVector2D& OutPosition);

	/** 将标记方位投影到水平罗盘条 */
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics|Marker")
	static bool ProjectMarkerToCompass(const FSceneMarkerView& InMarker, float InFieldOfView, float InWidth, bool bClamp, float& OutPosition);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics|Marker")
	static bool ProjectMarkerToScreen(APlayerController* InPlayer, const FSceneMarkerView& InMarker, bool bPlayerViewportRelative, FVector2D& OutPosition);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static UWorldTimer* GetWorldTimer(const UObject* WorldContextObject, TSubclassOf<UWorldTimer> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static UWorldWeather* GetWorldWeather(const UObject* WorldContextObject, TSubclassOf<UWorldWeather> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool HasTraceMapping(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static FTraceMapping GetTraceMapping(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddTraceMapping(const UObject* WorldContextObject, const FName InName, ECollisionChannel InTraceChannel);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveTraceMapping(const UObject* WorldContextObject, const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "InOnLoadFinished"), Category = "SceneModuleStatics")
	static void AsyncLoadLevel(const UObject* WorldContextObject, const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "InOnLoadFinished"), Category = "SceneModuleStatics")
	static void AsyncLoadLevelByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "InOnUnloadFinished"), Category = "SceneModuleStatics")
	static void AsyncUnloadLevel(const UObject* WorldContextObject, const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "InOnUnloadFinished"), Category = "SceneModuleStatics")
	static void AsyncUnloadLevelByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetAsyncLoadLevelProgress(const UObject* WorldContextObject, const FName InLevelPath);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetAsyncLoadLevelProgressByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetAsyncUnloadLevelProgress(const UObject* WorldContextObject, const FName InLevelPath);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static float GetAsyncUnloadLevelProgressByObjectPtr(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld> InLevelObjectPtr);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasSceneActor(const UObject* WorldContextObject, const FString& InID, bool bEnsured = true);

	template<class T>
	static T* GetSceneActor(const FString& InID, bool bEnsured = true)
	{
		return Cast<T>(GetSceneActor(InID, nullptr, bEnsured));
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static AActor* GetSceneActor(const UObject* WorldContextObject, const FString& InID, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool AddSceneActor(const UObject* WorldContextObject, AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool RemoveSceneActor(const UObject* WorldContextObject, AActor* InActor);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasTargetPointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static class ATargetPoint* GetTargetPointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddTargetPointByName(const UObject* WorldContextObject, const FName InName, class ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveTargetPointByName(const UObject* WorldContextObject, const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasScenePointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static USceneComponent* GetScenePointByName(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddScenePointByName(const UObject* WorldContextObject, const FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveScenePointByName(const UObject* WorldContextObject, const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static APhysicsVolume* GetDefaultPhysicsVolume(const UObject* WorldContextObject);
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasPhysicsVolumeByClass(const UObject* WorldContextObject, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool HasPhysicsVolumeByName(const UObject* WorldContextObject, const FName InName, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = InClass), Category = "SceneModuleStatics")
	static APhysicsVolumeBase* GetPhysicsVolumeByClass(const UObject* WorldContextObject, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = InClass), Category = "SceneModuleStatics")
	static APhysicsVolumeBase* GetPhysicsVolumeByName(const UObject* WorldContextObject, const FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddPhysicsVolume(const UObject* WorldContextObject, APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddPhysicsVolumeByName(const UObject* WorldContextObject, const FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemovePhysicsVolume(const UObject* WorldContextObject, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemovePhysicsVolumeByName(const UObject* WorldContextObject, const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void SpawnWorldText(const UObject* WorldContextObject, const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange = FVector::ZeroVector);
};
