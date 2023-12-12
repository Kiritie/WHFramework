// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SceneModuleTypes.h"
#include "Container/SceneContainerInterface.h"

#include "Main/Base/ModuleBase.h"
#include "Actor/PhysicsVolume/PhysicsVolumeTypes.h"

#include "SceneModule.generated.h"

class UWorldWeather;
class UWorldTimer;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API USceneModule : public UModuleBase, public ISceneContainerInterface
{
	GENERATED_BODY()
		
	GENERATED_MODULE(USceneModule)

public:
	USceneModule();

	~USceneModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;
	
	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	//////////////////////////////////////////////////////////////////////////
	// WorldTimer
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldWeather")
	UWorldTimer* WorldTimer;

public:
	UFUNCTION(BlueprintPure)
	UWorldTimer* GetWorldTimer() const { return WorldTimer; }

	//////////////////////////////////////////////////////////////////////////
	// WorldWeather
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldWeather")
	UWorldWeather* WorldWeather;
	
public:
	UFUNCTION(BlueprintPure)
	UWorldWeather* GetWorldWeather() const { return WorldWeather; }

	//////////////////////////////////////////////////////////////////////////
    /// Scene Actor
protected:
	UPROPERTY(EditAnywhere, Category = "SceneActor")
	TArray<AActor*> SceneActors;

	UPROPERTY(VisibleAnywhere, Category = "SceneActor")
	TMap<FGuid, AActor*> SceneActorMap;

public:
	UFUNCTION(BlueprintPure)
	virtual bool HasSceneActor(const FString& InID, bool bEnsured = true) const override;

	template<class T>
	T* GetSceneActor(const FString& InID, bool bEnsured = true) const
	{
		return Cast<T>(GetSceneActor(InID, nullptr, bEnsured));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true) const override;

	UFUNCTION(BlueprintCallable)
	virtual bool AddSceneActor(AActor* InActor) override;

	UFUNCTION(BlueprintCallable)
	virtual bool RemoveSceneActor(AActor* InActor) override;

	//////////////////////////////////////////////////////////////////////////
    /// Target Point
protected:
	UPROPERTY(EditAnywhere, Category = "TargetPoint")
	TMap<FName, ATargetPoint*> TargetPoints;

public:
	UFUNCTION(BlueprintPure)
	bool HasTargetPointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	ATargetPoint* GetTargetPointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	void AddTargetPointByName(FName InName, ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable)
	void RemoveTargetPointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// Scene Point
protected:
	UPROPERTY(EditAnywhere, Category = "ScenePoint")
	TMap<FName, USceneComponent*> ScenePoints;

public:
	UFUNCTION(BlueprintPure)
	bool HasScenePointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	USceneComponent* GetScenePointByName(FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	void AddScenePointByName(FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable)
	void RemoveScenePointByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TMap<FName, APhysicsVolumeBase*> PhysicsVolumes;

	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TArray<FPhysicsVolumeData> DefaultPhysicsVolumes;

public:
	UFUNCTION(BlueprintPure)
	bool HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true) const;
	
	UFUNCTION(BlueprintPure)
	bool HasPhysicsVolumeByName(FName InName, bool bEnsured = true) const;
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass))
	APhysicsVolumeBase* GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass))
	APhysicsVolumeBase* GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable)
	void AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable)
	void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable)
	void RemovePhysicsVolumeByName(FName InName);

	//////////////////////////////////////////////////////////////////////////
    /// World Text
public:
	UFUNCTION(BlueprintCallable)
	void SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetBindInfo InBindInfo, FVector InOffsetRange = FVector::ZeroVector);

	//////////////////////////////////////////////////////////////////////////
	/// Outline
protected:
	UPROPERTY(EditAnywhere, Category = "Outline")
	class UMaterialInterface* OutlineMat;

	UPROPERTY(EditAnywhere, Category = "Outline")
	FLinearColor OutlineColor;

	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* OutlineMatInst;

public:
	UFUNCTION(BlueprintPure)
	FLinearColor GetOutlineColor() const;

	UFUNCTION(BlueprintCallable)
	void SetOutlineColor(const FLinearColor& InColor);

	//////////////////////////////////////////////////////////////////////////
	/// Level
protected:
	UPROPERTY(VisibleAnywhere, Transient, Category = "Level")
	TMap<FName, TSoftObjectPtr<UWorld>> LoadedLevels;
	
public:
	UFUNCTION(BlueprintCallable)
	void AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable)
	void AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure)
	float GetAsyncLoadLevelProgress(FName InLevelPath) const;

	UFUNCTION(BlueprintPure)
	float GetAsyncUnloadLevelProgress(FName InLevelPath) const;

protected:
	UFUNCTION()
	void OnAsyncLoadLevelFinished(FName InLevelPath, const FOnAsyncLoadLevelFinished InOnAsyncLoadLevelFinished);

	UFUNCTION()
	void OnAsyncUnloadLevelFinished(FName InLevelPath, const FOnAsyncUnloadLevelFinished InOnAsyncUnloadLevelFinished);
};
