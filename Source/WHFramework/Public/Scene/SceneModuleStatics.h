
#pragma once

#include "SceneModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Widget/WidgetModuleTypes.h"

#include "SceneModuleStatics.generated.h"

class AMiniMapCapture;
class UWorldWeather;
class UWorldTimer;
class APhysicsVolumeBase;
class APhysicsVolume;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USceneModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetSeaLevel();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SetSeaLevel(float InSeaLevel);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAltitude(bool bUnsigned = false, bool bRefresh = false);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static AMiniMapCapture* GetMiniMapCapture();

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static EWorldMiniMapMode GetMiniMapMode();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SetMiniMapMode(EWorldMiniMapMode InMiniMapMode);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static FTransform GetMiniMapPoint();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SetMiniMapPoint(const FTransform& InMiniMapPoint);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetMiniMapRange();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SetMiniMapRange(float InMiniMapRange);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static UTextureRenderTarget2D* GetMiniMapTexture();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SetMiniMapTexture(UTextureRenderTarget2D* InMiniMapTexture);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasSceneArea(const FName InName);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static FSceneArea GetSceneArea(const FName InName);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static FSceneArea GetSceneAreaByPoint(const FVector2D& InPoint);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static TArray<FSceneArea> GetSceneAreas();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddSceneArea(const FSceneArea& InArea, bool bThreadSafe = false);
	
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemoveSceneArea(const FName InName);
		
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void ClearSceneArea();

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static UWorldTimer* GetWorldTimer(TSubclassOf<UWorldTimer> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static UWorldWeather* GetWorldWeather(TSubclassOf<UWorldWeather> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure)
	static bool HasTraceMapping(const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static FTraceMapping GetTraceMapping(const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddTraceMapping(const FName InName, ECollisionChannel InTraceChannel);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemoveTraceMapping(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnLoadFinished"), Category = "SceneModuleStatics")
	static void AsyncLoadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnLoadFinished"), Category = "SceneModuleStatics")
	static void AsyncLoadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnUnloadFinished"), Category = "SceneModuleStatics")
	static void AsyncUnloadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnUnloadFinished"), Category = "SceneModuleStatics")
	static void AsyncUnloadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAsyncLoadLevelProgress(const FName InLevelPath);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAsyncLoadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAsyncUnloadLevelProgress(const FName InLevelPath);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAsyncUnloadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasSceneActor(const FString& InID, bool bEnsured = true);

	template<class T>
	static T* GetSceneActor(const FString& InID, bool bEnsured = true)
	{
		return Cast<T>(GetSceneActor(InID, nullptr, bEnsured));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static AActor* GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static bool AddSceneActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static bool RemoveSceneActor(AActor* InActor);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasTargetPointByName(const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static class ATargetPoint* GetTargetPointByName(const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddTargetPointByName(const FName InName, class ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemoveTargetPointByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasScenePointByName(const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static USceneComponent* GetScenePointByName(const FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddScenePointByName(const FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemoveScenePointByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static APhysicsVolume* GetDefaultPhysicsVolume();
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasPhysicsVolumeByName(const FName InName, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleStatics")
	static APhysicsVolumeBase* GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleStatics")
	static APhysicsVolumeBase* GetPhysicsVolumeByName(const FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddPhysicsVolumeByName(const FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemovePhysicsVolumeByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SpawnWorldText(const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange = FVector::ZeroVector);
};
