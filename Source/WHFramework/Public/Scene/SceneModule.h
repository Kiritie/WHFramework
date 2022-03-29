// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneModuleTypes.h"

#include "Main/Base/ModuleBase.h"
#include "Object/PhysicsVolume/PhysicsVolumeTypes.h"

#include "SceneModule.generated.h"

class UWorldWeatherComponent;
class UWorldTimerComponent;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API ASceneModule : public AModuleBase
{
	GENERATED_BODY()

public:
	ASceneModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;
	
	//////////////////////////////////////////////////////////////////////////
	// Components
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UWorldTimerComponent* WorldTimer;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UWorldWeatherComponent* WorldWeather;
public:
	UFUNCTION(BlueprintPure)
	UWorldTimerComponent* GetWorldTimer() const { return WorldTimer; }

	UFUNCTION(BlueprintPure)
	UWorldWeatherComponent* GetWorldWeather() const { return WorldWeather; }

	//////////////////////////////////////////////////////////////////////////
	/// Level
protected:
	UPROPERTY(VisibleAnywhere, Transient, Category = "TargetPoint")
	TMap<FName, TSoftObjectPtr<UWorld>> LoadedLevels;
	
public:
	UFUNCTION(BlueprintCallable, Category = "SceneObject")
	void AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, Category = "SceneObject")
	void AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure, Category = "SceneObject")
	float GetAsyncLoadLevelProgress(FName InLevelPath) const;

	UFUNCTION(BlueprintPure, Category = "SceneObject")
	float GetAsyncUnloadLevelProgress(FName InLevelPath) const;

protected:
	UFUNCTION()
	void OnAsyncLoadLevelFinished(FName InLevelPath, const FOnAsyncLoadLevelFinished InOnAsyncLoadLevelFinished);

	UFUNCTION()
	void OnAsyncUnloadLevelFinished(FName InLevelPath, const FOnAsyncUnloadLevelFinished InOnAsyncUnloadLevelFinished);

	//////////////////////////////////////////////////////////////////////////
    /// Scene Object
protected:
	UPROPERTY(EditAnywhere, Category = "SceneObject")
	TMap<FName, TScriptInterface<class ISceneObjectInterface>> SceneObjects;

public:
	UFUNCTION(BlueprintPure, Category = "SceneObject")
	bool HasSceneObject(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "SceneObject")
	TScriptInterface<class ISceneObjectInterface> GetSceneObject(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "TargetPoint")
	void SetSceneObject(FName InName, TScriptInterface<class ISceneObjectInterface> InObject);

	//////////////////////////////////////////////////////////////////////////
    /// Target Point
protected:
	UPROPERTY(EditAnywhere, Category = "TargetPoint")
	TMap<FName, class ATargetPoint*> TargetPoints;

public:
	UFUNCTION(BlueprintPure, Category = "TargetPoint")
	bool HasTargetPoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "TargetPoint")
	ATargetPoint* GetTargetPoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "TargetPoint")
	void SetTargetPoint(FName InName, ATargetPoint* InPoint);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
protected:
	UPROPERTY(EditAnywhere, Category = "ScenePoint")
	TMap<FName, USceneComponent*> ScenePoints;

public:
	UFUNCTION(BlueprintPure, Category = "ScenePoint")
	bool HasScenePoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "ScenePoint")
	USceneComponent* GetScenePoint(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "ScenePoint")
	void SetScenePoint(FName InName, USceneComponent* InSceneComp);

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TMap<FName, class APhysicsVolumeBase*> PhysicsVolumes;

	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TArray<FPhysicsVolumeData> DefaultPhysicsVolumes;

public:
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	bool HasPhysicsVolume(FName InName, bool bEnsured = true) const;

	virtual APhysicsVolume* GetPhysicsVolume() const override;
	
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	class APhysicsVolumeBase* GetPhysicsVolume(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void SetPhysicsVolume(FName InName, class APhysicsVolumeBase* InPhysicsVolume);
	
	//////////////////////////////////////////////////////////////////////////
	/// Outline
protected:
	UPROPERTY(EditAnywhere)
	class UMaterialInterface* OutlineMat;

	UPROPERTY(EditAnywhere)
	FLinearColor OutlineColor;

	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* OutlineMatInst;

public:
	UFUNCTION(BlueprintPure)
	FLinearColor GetOutlineColor() const;

	UFUNCTION(BlueprintCallable)
	void SetOutlineColor(const FLinearColor& InColor);
};
