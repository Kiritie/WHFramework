// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SceneModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Widget/WidgetModuleTypes.h"

#include "SceneModuleStatics.generated.h"

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
	/// World
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetSeaLevel();

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SetSeaLevel(float InSeaLevel);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAltitude(bool bUnsigned = false, bool bRefresh = false);

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
	/// WorldTimer
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static UWorldTimer* GetWorldTimer(TSubclassOf<UWorldTimer> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	/// WorldWeather
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "SceneModuleStatics")
	static UWorldWeather* GetWorldWeather(TSubclassOf<UWorldWeather> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	/// Level
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnAsyncLoadLevelFinished"), Category = "SceneModuleStatics")
	static void AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnAsyncUnloadLevelFinished"), Category = "SceneModuleStatics")
	static void AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAsyncLoadLevelProgress(FName InLevelPath);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static float GetAsyncUnloadLevelProgress(FName InLevelPath);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Actor
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
	/// Target Point
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasTargetPointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static class ATargetPoint* GetTargetPointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddTargetPointByName(FName InName, class ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemoveTargetPointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasScenePointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static USceneComponent* GetScenePointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddScenePointByName(FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemoveScenePointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Physics Volume
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static APhysicsVolume* GetDefaultPhysicsVolume();
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleStatics")
	static bool HasPhysicsVolumeByName(FName InName, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleStatics")
	static APhysicsVolumeBase* GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleStatics")
	static APhysicsVolumeBase* GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void RemovePhysicsVolumeByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
    /// World Text
	UFUNCTION(BlueprintCallable, Category = "SceneModuleStatics")
	static void SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange = FVector::ZeroVector);
};
