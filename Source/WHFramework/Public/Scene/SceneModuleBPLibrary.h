// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "SceneModuleBPLibrary.generated.h"

class APhysicsVolumeBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USceneModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
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
	static bool HasSceneActor(TSubclassOf<AActor> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasSceneActorByName(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleBPLibrary")
	static AActor* GetSceneActor(TSubclassOf<AActor> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneModuleBPLibrary")
	static AActor* GetSceneActorByName(FName InName, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddSceneActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddSceneActorByName(FName InName, AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemoveSceneActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemoveSceneActorByName(FName InName);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void DestroySceneActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void DestroySceneActorByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Target Point
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasTargetPoint(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static class ATargetPoint* GetTargetPoint(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddTargetPoint(FName InName, class ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemoveTargetPoint(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static bool HasScenePoint(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintPure, Category = "SceneModuleBPLibrary")
	static USceneComponent* GetScenePoint(FName InName, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void AddScenePoint(FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable, Category = "SceneModuleBPLibrary")
	static void RemoveScenePoint(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Physics Volume
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	bool HasPhysicsVolume(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	bool HasPhysicsVolumeByName(FName InName, bool bEnsured = true);
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "PhysicsVolumes")
	APhysicsVolumeBase* GetPhysicsVolume(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "PhysicsVolumes")
	APhysicsVolumeBase* GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true);

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void RemovePhysicsVolumeByName(FName InName);
};
