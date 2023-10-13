// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SceneModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Widget/WidgetModuleTypes.h"

#include "SceneModuleBPLibrary.generated.h"

class UWorldWeatherComponent;
class UWorldTimerComponent;
class APhysicsVolumeBase;
class APhysicsVolume;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USceneModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	/// Components
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static UWorldTimerComponent* GetWorldTimer();

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static UWorldWeatherComponent* GetWorldWeather();

	//////////////////////////////////////////////////////////////////////////
	/// Level
	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static float GetAsyncLoadLevelProgress(FName InLevelPath);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static float GetAsyncUnloadLevelProgress(FName InLevelPath);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Actor
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasSceneActor(FGuid InID, bool bEnsured = true);

	template<class T>
	static T* GetSceneActor(FGuid InID, bool bEnsured = true)
	{
		return Cast<T>(GetSceneActor(InID, nullptr, bEnsured));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleBPLibrary")
	static AActor* GetSceneActor(FGuid InID, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static bool AddSceneActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static bool RemoveSceneActor(AActor* InActor);

	//////////////////////////////////////////////////////////////////////////
	/// Target Point
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasTargetPointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static class ATargetPoint* GetTargetPointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddTargetPointByName(FName InName, class ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemoveTargetPointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasScenePointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static USceneComponent* GetScenePointByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddScenePointByName(FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemoveScenePointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Physics Volume
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static APhysicsVolume* GetDefaultPhysicsVolume();
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasPhysicsVolumeByName(FName InName, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleBPLibrary")
	static APhysicsVolumeBase* GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleBPLibrary")
	static APhysicsVolumeBase* GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemovePhysicsVolumeByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
    /// World Text
	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetBindInfo InBindInfo, FVector InOffsetRange = FVector::ZeroVector);
};
