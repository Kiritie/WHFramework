#pragma once

#include "SceneModuleTypes.h"
#include "Container/SceneContainerInterface.h"

#include "Main/Base/ModuleBase.h"
#include "Actor/PhysicsVolume/PhysicsVolumeTypes.h"
#include "Debug/DebuggerInterface.h"

#include "SceneModule.generated.h"

class UEventHandle_StopLevelSequence;
class UEventHandle_AsyncUnloadLevels;
class UEventHandle_AsyncLoadLevels;
class UEventHandle_PlayLevelSequence;
class UEventHandle_SetActorVisible;
class UEventHandle_SetDataLayerOwnerPlayer;
class ATargetPoint;
class AMiniMapCapture;
class UEventHandle_SetDataLayerRuntimeState;
class UWorldWeather;
class UWorldTimer;
class UWidgetSceneWorldMarker;
/** 场景模块 */
UCLASS()
class WHFRAMEWORK_API USceneModule : public UModuleBase, public ISceneContainerInterface, public IDebuggerInterface
{
	GENERATED_BODY()

	GENERATED_MODULE(USceneModule)

public:
	USceneModule();

	~USceneModule();

	//////////////////////////////////////////////////////////////////////////
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

	//////////////////////////////////////////////////////////////////////////
protected:
	virtual void OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC) override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "bModuleAutoSave == true"))
	bool bSaveActorDatas;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "Coordinate")
	float SeaLevel;
	
	UPROPERTY(VisibleAnywhere, Category = "Coordinate")
	float Altitude;

public:
	UFUNCTION(BlueprintPure)
	float GetSeaLevel() const { return SeaLevel; }
	
	UFUNCTION(BlueprintCallable)
	void SetSeaLevel(float InSeaLevel) { SeaLevel = InSeaLevel; }

	UFUNCTION(BlueprintPure)
	float GetAltitude(bool bUnsigned = false, bool bRefresh = false) const;
	
	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "MiniMap")
	AMiniMapCapture* MiniMapCapture;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	bool bMiniMapRotatable;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	EWorldMiniMapMode MiniMapMode;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "MiniMapMode == EWorldMiniMapMode::FixedPoint"), Category = "MiniMap")
	FTransform MiniMapPoint;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	float MiniMapRange;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	float MiniMapMinRange;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	float MiniMapMaxRange;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	UTextureRenderTarget2D* MiniMapTexture;

public:
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
	/// 世界地图
protected:
	UPROPERTY(EditAnywhere, Category = "WorldMap")
	FVector2D WorldMapCenter;

	UPROPERTY(EditAnywhere, Category = "WorldMap")
	float WorldMapRange;

	UPROPERTY(EditAnywhere, Category = "WorldMap")
	float WorldMapMinRange;

	UPROPERTY(EditAnywhere, Category = "WorldMap")
	float WorldMapMaxRange;

public:
	UFUNCTION(BlueprintPure)
	FVector2D GetWorldMapCenter() const { return WorldMapCenter; }

	UFUNCTION(BlueprintCallable)
	void SetWorldMapCenter(FVector2D InCenter) { WorldMapCenter = InCenter; }

	UFUNCTION(BlueprintPure)
	float GetWorldMapRange() const { return WorldMapRange; }

	UFUNCTION(BlueprintCallable)
	void SetWorldMapRange(float InRange) { WorldMapRange = FMath::Clamp(InRange, WorldMapMinRange, WorldMapMaxRange); }

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "SceneArea")
	TArray<FSceneArea> SceneAreas;

	TQueue<FSceneArea, EQueueMode::Mpsc> PendingSceneAreas;

	UPROPERTY(EditAnywhere, Category = "SceneArea")
	bool bDrawSceneArea;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bDrawSceneArea == true"), Category = "SceneArea")
	float SceneAreaHeight;

	TMap<ESceneAreaType, FSceneAreaResolver> SceneAreaResolvers;

public:
	FOnSceneAreaAdded OnSceneAreaAdded;

	UFUNCTION(BlueprintPure)
	bool HasSceneArea(const FName InName) const;

	UFUNCTION(BlueprintPure)
	FSceneArea GetSceneArea(const FName InName) const;

	UFUNCTION(BlueprintPure)
	FSceneArea GetSceneAreaByPoint(const FVector2D& InPoint) const;

	UFUNCTION(BlueprintPure)
	TArray<FSceneArea> GetSceneAreas() const { return SceneAreas; }

	void RegisterSceneAreaResolver(ESceneAreaType InType, const FSceneAreaResolver& InResolver);

	void UnregisterSceneAreaResolver(ESceneAreaType InType);

	UFUNCTION(BlueprintCallable)
	void AddSceneArea(const FSceneArea& InArea, bool bThreadSafe = false);
	
	UFUNCTION(BlueprintCallable)
	void RemoveSceneArea(const FName InName);
		
	UFUNCTION(BlueprintCallable)
	void ClearSceneArea();

	//////////////////////////////////////////////////////////////////////////
	/// 标记
protected:
	UPROPERTY(VisibleAnywhere, Category = "Marker")
	TMap<FGuid, FSceneMarker> Markers;

	UPROPERTY(VisibleAnywhere, Category = "Marker")
	FGuid TrackedMarkerID;

	UPROPERTY(Transient)
	TMap<FGuid, UWidgetSceneWorldMarker*> WorldMarkerWidgets;

public:
	UPROPERTY(BlueprintAssignable)
	FOnSceneMarkersChanged OnSceneMarkersChanged;

	UFUNCTION(BlueprintCallable)
	FGuid AddMarker(const FSceneMarker& InMarker);

	UFUNCTION(BlueprintCallable)
	bool UpdateMarker(const FSceneMarker& InMarker);

	UFUNCTION(BlueprintCallable)
	bool RemoveMarker(FGuid InMarkerID);

	UFUNCTION(BlueprintCallable)
	void ClearMarkers(bool bIncludePersistent = true);

	UFUNCTION(BlueprintPure)
	bool HasMarker(FGuid InMarkerID) const { return Markers.Contains(InMarkerID); }

	UFUNCTION(BlueprintPure)
	FSceneMarker GetMarker(FGuid InMarkerID) const;

	UFUNCTION(BlueprintPure)
	TArray<FSceneMarkerView> GetMarkerViews(ESceneMarkerChannel InChannel, FVector InViewLocation, float InViewYaw = 0.f) const;

	UFUNCTION(BlueprintCallable)
	bool SetTrackedMarker(FGuid InMarkerID);

	UFUNCTION(BlueprintPure)
	FGuid GetTrackedMarker() const { return TrackedMarkerID; }

private:
	FSceneMarkerView ResolveMarker(const FSceneMarker& InMarker, const FVector& InViewLocation, float InViewYaw) const;
	void RefreshWorldMarkerWidgets();
	void ClearWorldMarkerWidgets();
	UPROPERTY(Transient)
	FSceneModuleSaveData CachedSaveData;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldTimer")
	UWorldTimer* WorldTimer;

public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldTimer* GetWorldTimer(TSubclassOf<UWorldTimer> InClass = nullptr) const;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldWeather")
	UWorldWeather* WorldWeather;
	
public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldWeather* GetWorldWeather(TSubclassOf<UWorldWeather> InClass = nullptr) const;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "DataLayer")
	TMap<UDataLayerAsset*, int32> DataLayerPlayerMappings;
	
	UPROPERTY(EditAnywhere, Category = "DataLayer")
	TMap<FName, int32> LevelPlayerMappings;
	
protected:
	UFUNCTION()
	void OnAsyncLoadLevels(UObject* InSender, UEventHandle_AsyncLoadLevels* InEventHandle);

	UFUNCTION()
	void OnAsyncUnloadLevels(UObject* InSender, UEventHandle_AsyncUnloadLevels* InEventHandle);

	UFUNCTION()
	void OnSetActorVisible(UObject* InSender, UEventHandle_SetActorVisible* InEventHandle);
	
	UFUNCTION()
	void OnPlayLevelSequence(UObject* InSender, UEventHandle_PlayLevelSequence* InEventHandle);
		
	UFUNCTION()
	void OnStopLevelSequence(UObject* InSender, UEventHandle_StopLevelSequence* InEventHandle);

	UFUNCTION()
	void OnSetDataLayerRuntimeState(UObject* InSender, UEventHandle_SetDataLayerRuntimeState* InEventHandle);
	
	UFUNCTION()
	void OnSetDataLayerOwnerPlayer(UObject* InSender, UEventHandle_SetDataLayerOwnerPlayer* InEventHandle);
	
	UFUNCTION()
	void OnSetLevelOwnerPlayer(UObject* InSender, UEventHandle_SetLevelOwnerPlayer* InEventHandle);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "Traces")
	TMap<FName, FTraceMapping> TraceMappings;

public:
	UFUNCTION(BlueprintPure)
	virtual bool HasTraceMapping(const FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	virtual FTraceMapping GetTraceMapping(const FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	virtual void AddTraceMapping(const FName InName, ECollisionChannel InTraceChannel);

	UFUNCTION(BlueprintCallable)
	virtual void RemoveTraceMapping(const FName InName);

	//////////////////////////////////////////////////////////////////////////
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
protected:
	UPROPERTY(EditAnywhere, Category = "TargetPoint")
	TMap<FName, ATargetPoint*> TargetPoints;

public:
	UFUNCTION(BlueprintPure)
	bool HasTargetPointByName(const FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	ATargetPoint* GetTargetPointByName(const FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	void AddTargetPointByName(const FName InName, ATargetPoint* InPoint);

	UFUNCTION(BlueprintCallable)
	void RemoveTargetPointByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "ScenePoint")
	TMap<FName, USceneComponent*> ScenePoints;

public:
	UFUNCTION(BlueprintPure)
	bool HasScenePointByName(const FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	USceneComponent* GetScenePointByName(const FName InName, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	void AddScenePointByName(const FName InName, USceneComponent* InSceneComp);

	UFUNCTION(BlueprintCallable)
	void RemoveScenePointByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TMap<FName, APhysicsVolumeBase*> PhysicsVolumes;

	UPROPERTY(EditAnywhere, Category = "PhysicsVolumes")
	TArray<FPhysicsVolumeData> DefaultPhysicsVolumes;

public:
	UFUNCTION(BlueprintPure)
	bool HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true) const;
	
	UFUNCTION(BlueprintPure)
	bool HasPhysicsVolumeByName(const FName InName, bool bEnsured = true) const;
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass))
	APhysicsVolumeBase* GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = InClass))
	APhysicsVolumeBase* GetPhysicsVolumeByName(const FName InName, TSubclassOf<APhysicsVolumeBase> InClass = nullptr, bool bEnsured = true) const;

	UFUNCTION(BlueprintCallable)
	void AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);
	
	UFUNCTION(BlueprintCallable)
	void AddPhysicsVolumeByName(const FName InName, APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable)
	void RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume);

	UFUNCTION(BlueprintCallable)
	void RemovePhysicsVolumeByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
public:
	UFUNCTION(BlueprintCallable)
	void SpawnWorldText(const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange = FVector::ZeroVector);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "Outline")
	UMaterialInterface* OutlineMat;

	UPROPERTY(EditAnywhere, Category = "Outline")
	FLinearColor OutlineColor;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* OutlineMatInst;

public:
	UFUNCTION(BlueprintPure)
	FLinearColor GetOutlineColor() const;

	UFUNCTION(BlueprintCallable)
	void SetOutlineColor(const FLinearColor& InColor);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(VisibleAnywhere, Category = "Level")
	TArray<FAsyncLoadLevelTask> AsyncLoadLevelQueue;

public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnLoadFinished"))
	void AsyncLoadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnLoadFinished"))
	void AsyncLoadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnUnloadFinished"))
	void AsyncUnloadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InOnUnloadFinished"))
	void AsyncUnloadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime = 1.f, bool bCreateLoadingWidget = true);

	UFUNCTION(BlueprintPure)
	float GetAsyncLoadLevelProgress(const FName InLevelPath) const;

	UFUNCTION(BlueprintPure)
	float GetAsyncLoadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr) const;

	UFUNCTION(BlueprintPure)
	float GetAsyncUnloadLevelProgress(const FName InLevelPath) const;

	UFUNCTION(BlueprintPure)
	float GetAsyncUnloadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr) const;

protected:
	UFUNCTION(BlueprintCallable)
	void AsyncLoadLevelInternal(FAsyncLoadLevelTask& InTask);

	UFUNCTION(BlueprintCallable)
	void AsyncUnloadLevelInternal(FAsyncLoadLevelTask& InTask);

	UFUNCTION()
	void OnAsyncLoadLevelFinished(FAsyncLoadLevelTask InTask);

	UFUNCTION()
	void OnAsyncUnloadLevelFinished(FAsyncLoadLevelTask InTask);

private:
	UFUNCTION()
	void OnHandleAsyncLoadLevelFinish();
	
	UFUNCTION()
	void OnHandleAsyncUnloadLevelFinish();
};
