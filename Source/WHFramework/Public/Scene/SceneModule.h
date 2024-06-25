// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "SceneModuleTypes.h"
#include "Container/SceneContainerInterface.h"

#include "Main/Base/ModuleBase.h"
#include "Actor/PhysicsVolume/PhysicsVolumeTypes.h"

#include "SceneModule.generated.h"

class UEventHandle_SetDataLayerOwnerPlayer;
class ATargetPoint;
class AMiniMapCapture;
class UEventHandle_SetDataLayerRuntimeState;
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
	/// ModuleBase
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds, bool bInEditor) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;
	
	virtual void OnTermination(EPhase InPhase) override;
	
protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;
	
public:
	virtual FString GetModuleDebugMessage() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//////////////////////////////////////////////////////////////////////////
	// World
protected:
	UPROPERTY(EditAnywhere, Category = "World|Coordinate")
	float SeaLevel;
	
	UPROPERTY(VisibleAnywhere, Category = "World|Coordinate")
	float Altitude;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "World|MiniMap")
	AMiniMapCapture* MiniMapCapture;

	UPROPERTY(EditAnywhere, Category = "World|MiniMap")
	bool bMiniMapRotatable;

	UPROPERTY(EditAnywhere, Category = "World|MiniMap")
	EWorldMiniMapMode MiniMapMode;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "MiniMapMode == EWorldMiniMapMode::FixedPoint"), Category = "World|MiniMap")
	FTransform MiniMapPoint;

	UPROPERTY(EditAnywhere, Category = "World|MiniMap")
	float MiniMapRange;

	UPROPERTY(EditAnywhere, Category = "World|MiniMap")
	float MiniMapMinRange;

	UPROPERTY(EditAnywhere, Category = "World|MiniMap")
	float MiniMapMaxRange;

	UPROPERTY(EditAnywhere, Category = "World|MiniMap")
	UTextureRenderTarget2D* MiniMapTexture;

public:
	UFUNCTION(BlueprintPure)
	float GetSeaLevel() const { return SeaLevel; }
	
	UFUNCTION(BlueprintCallable)
	void SetSeaLevel(float InSeaLevel) { SeaLevel = InSeaLevel; }

	UFUNCTION(BlueprintPure)
	float GetAltitude(bool bUnsigned = false, bool bRefresh = false) const;

	UFUNCTION(BlueprintPure)
	AMiniMapCapture* GetMiniMapCapture() const { return MiniMapCapture; }

	UFUNCTION(BlueprintPure)
	EWorldMiniMapMode GetMiniMapMode() const { return MiniMapMode; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapMode(EWorldMiniMapMode InMiniMapMode);

	UFUNCTION(BlueprintPure)
	FTransform GetMiniMapPoint() const { return MiniMapPoint; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapPoint(const FTransform& InMiniMapPoint) { MiniMapPoint = InMiniMapPoint; }

	UFUNCTION(BlueprintPure)
	float GetMiniMapRange() const { return MiniMapRange; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapRange(float InMiniMapRange) { MiniMapRange = FMath::Clamp(InMiniMapRange, MiniMapMinRange, MiniMapMaxRange != -1.f ? MiniMapMaxRange : InMiniMapRange); }

	UFUNCTION(BlueprintPure)
	UTextureRenderTarget2D* GetMiniMapTexture() const { return MiniMapTexture; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapTexture(UTextureRenderTarget2D* InMiniMapTexture);

	//////////////////////////////////////////////////////////////////////////
	// WorldTimer
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldTimer")
	UWorldTimer* WorldTimer;

public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldTimer* GetWorldTimer(TSubclassOf<UWorldTimer> InClass = nullptr) const;

	//////////////////////////////////////////////////////////////////////////
	// WorldWeather
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldWeather")
	UWorldWeather* WorldWeather;
	
public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldWeather* GetWorldWeather(TSubclassOf<UWorldWeather> InClass = nullptr) const;

	//////////////////////////////////////////////////////////////////////////
	// DataLayer
protected:
	UPROPERTY(EditAnywhere, Category = "DataLayer")
	TMap<UDataLayerAsset*, int32> DataLayerPlayerMappings;
	
protected:
	UFUNCTION()
	void OnSetDataLayerRuntimeState(UObject* InSender, UEventHandle_SetDataLayerRuntimeState* InEventHandle);
	
	UFUNCTION()
	void OnSetDataLayerOwnerPlayer(UObject* InSender, UEventHandle_SetDataLayerOwnerPlayer* InEventHandle);

	//////////////////////////////////////////////////////////////////////////
	/// Traces
protected:
	UPROPERTY(EditAnywhere, Category = "Traces")
	TMap<FName, FTraceMapping> TraceMappings;

public:
	UFUNCTION(BlueprintPure)
	virtual bool HasTraceMapping(const FName& InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	virtual FTraceMapping GetTraceMapping(const FName& InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	virtual void AddTraceMapping(const FName& InName, ECollisionChannel InTraceChannel);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveTraceMapping(const FName& InName);

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
	void SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange = FVector::ZeroVector);

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
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnAsyncLoadLevelFinished"))
	void AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnAsyncUnloadLevelFinished"))
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
