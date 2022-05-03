// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneModuleTypes.h"
#include "Container/SceneContainerInterface.h"

#include "Main/Base/ModuleBase.h"
#include "Actor/PhysicsVolume/PhysicsVolumeTypes.h"

#include "SceneModule.generated.h"

class UWorldWeatherComponent;
class UWorldTimerComponent;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API ASceneModule : public AModuleBase, public ISceneContainerInterface
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
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UWorldTimerComponent* WorldTimer;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
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
	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	void AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	void AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure, Category = "SceneActor")
	float GetAsyncLoadLevelProgress(FName InLevelPath) const;

	UFUNCTION(BlueprintPure, Category = "SceneActor")
	float GetAsyncUnloadLevelProgress(FName InLevelPath) const;

protected:
	UFUNCTION()
	void OnAsyncLoadLevelFinished(FName InLevelPath, const FOnAsyncLoadLevelFinished InOnAsyncLoadLevelFinished);

	UFUNCTION()
	void OnAsyncUnloadLevelFinished(FName InLevelPath, const FOnAsyncUnloadLevelFinished InOnAsyncUnloadLevelFinished);

	//////////////////////////////////////////////////////////////////////////
    /// Scene Actor
protected:
	UPROPERTY(EditAnywhere, Category = "SceneActor")
	TArray<AActor*> SceneActors;

	UPROPERTY(EditAnywhere, Category = "SceneActor")
	TMap<FName, AActor*> SceneActorMap;

public:
	UFUNCTION(BlueprintPure, Category = "SceneActor")
	virtual bool HasSceneActorByClass(TSubclassOf<AActor> InClass, bool bEnsured = true) const override;

	UFUNCTION(BlueprintPure, Category = "SceneActor")
	virtual bool HasSceneActorByName(FName InName, bool bEnsured = true) const override;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneActor")
	virtual AActor* GetSceneActorByClass(TSubclassOf<AActor> InClass, bool bEnsured = true) const override;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "SceneActor")
	virtual AActor* GetSceneActorByName(FName InName, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	virtual void AddSceneActor(AActor* InActor) override;

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	virtual void AddSceneActorByName(FName InName, AActor* InActor) override;

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	virtual void RemoveSceneActor(AActor* InActor) override;

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	virtual void RemoveSceneActorByName(FName InName) override;

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	virtual void DestroySceneActor(AActor* InActor) override;

	UFUNCTION(BlueprintCallable, Category = "SceneActor")
	virtual void DestroySceneActorByName(FName InName) override;

	//////////////////////////////////////////////////////////////////////////
    /// Target Point
protected:
	UPROPERTY(EditAnywhere, Category = "TargetPoint")
	TMap<FName, class ATargetPoint*> TargetPoints;

public:
	UFUNCTION(BlueprintPure, Category = "TargetPoint")
	bool HasTargetPointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "TargetPoint")
	ATargetPoint* GetTargetPointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "TargetPoint")
	void AddTargetPointByName(FName InName, ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable, Category = "TargetPoint")
	void RemoveTargetPointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
protected:
	UPROPERTY(EditAnywhere, Category = "ScenePoint")
	TMap<FName, USceneComponent*> ScenePoints;

public:
	UFUNCTION(BlueprintPure, Category = "ScenePoint")
	bool HasScenePointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, Category = "ScenePoint")
	USceneComponent* GetScenePointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "ScenePoint")
	void AddScenePointByName(FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable, Category = "ScenePoint")
	void RemoveScenePointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TMap<FName, APhysicsVolumeBase*> PhysicsVolumes;

	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TArray<FPhysicsVolumeData> DefaultPhysicsVolumes;

public:
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	bool HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true) const;
	
	UFUNCTION(BlueprintPure, Category = "PhysicsVolumes")
	bool HasPhysicsVolumeByName(FName InName, bool bEnsured = true) const;
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "PhysicsVolumes")
	APhysicsVolumeBase* GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass), Category = "PhysicsVolumes")
	APhysicsVolumeBase* GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable, Category = "PhysicsVolumes")
	void RemovePhysicsVolumeByName(FName InName);

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
