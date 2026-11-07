#pragma once

#include "Widget/WorldWidgetTypes.h"
#include "Common/LifecycleTypes.h"
#include "Scene/SceneMapTypes.h"
#include "Scene/SceneAreaTypes.h"
#include "Scene/SceneMarkerTypes.h"
#include "Scene/SceneStreamingTypes.h"
#include "Main/MainModuleAccess.h"
#include "HAL/CriticalSection.h"
#include "TimerManager.h"
#include "SceneModuleTypes.h"
#include "Container/SceneContainerInterface.h"

#include "Main/Base/ModuleBase.h"
#include "Actor/PhysicsVolume/PhysicsVolumeTypes.h"
#include "Debug/DebuggerInterface.h"

#include "SceneModule.generated.h"

struct FEventStopLevelSequence;
struct FEventAsyncUnloadLevels;
struct FEventAsyncLoadLevels;
struct FEventPlayLevelSequence;
struct FEventSetActorVisible;
struct FEventSetDataLayerOwnerPlayer;
class ATargetPoint;
class AMiniMapCapture;
class ULocalPlayer;
struct FEventSetDataLayerRuntimeState;
class UWorldWeather;
class UWorldTimer;
class UWidgetSceneWorldMarker;
class UWidgetSceneMapBase;
class ACameraStreamingBridge;
class ULevel;
class UPrimitiveComponent;

USTRUCT()
struct FScenePlayerWorldMarkers
{
	GENERATED_BODY()
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UWidgetSceneWorldMarker>> Widgets;
};

struct FDeferredSceneActorRegistration
{
	FGuid ExpectedId;
	FGuid PreviousId;
	uint64 Serial = 0;
};
struct FSceneOwnerComponentOverride
{
	bool bOriginalOnlyOwnerSee = false;
	bool bLastOnlyOwnerSee = false;
	bool bManaged = false;
	bool bOriginalVisible = false;
	bool bLastVisible = false;
	bool bVisibilityManaged = false;
};

struct FSceneOwnerActorOverride
{
	TWeakObjectPtr<AActor> OriginalOwner;
	TWeakObjectPtr<AActor> LastOwner;
	bool bManaged = false;
	bool bConflict = false;
	bool bOwnerRetry = false;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FSceneOwnerComponentOverride> Components;
};

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

	virtual void OnControlRefresh(float RealDeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;
	
	virtual void OnTermination(EPhase InPhase) override;
	
protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual bool WasLoadDataApplied() const override { return bLastSceneLoadSucceeded; }

	virtual FParameter ToData() override;

public:
	bool LoadSceneSaveData(const FParameter& InData, EPhase InPhase);

	virtual FString GetModuleDebugMessage() override;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, TObjectPtr<ACameraStreamingBridge>> CameraStreamingBridges;
	bool bRefreshingCameraStreamingBridges = false;
	bool bSceneRuntimeClosing = false;
	bool bSceneLoadInProgress = false;
	bool bLastSceneLoadSucceeded = false;
	uint32 CameraStreamingBridgeGeneration = 0;
	void RefreshCameraStreamingBridges();
	void ClearCameraStreamingBridges();

	virtual void OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bSaveActorDatas;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "Coordinate")
	float SeaLevel;
	
public:
	UFUNCTION(BlueprintPure)
	float GetSeaLevel() const { return SeaLevel; }
	
	UFUNCTION(BlueprintCallable)
	void SetSeaLevel(float InSeaLevel)
	{
		if (IsBusinessAdmissionOpen()) SeaLevel = InSeaLevel;
	}

	UFUNCTION(BlueprintPure)
	float GetAltitude(APlayerController* InPlayer, bool bUnsigned = false) const;
	
	//////////////////////////////////////////////////////////////////////////
private:
	void RefreshMiniMapCapture();
	void UpdateMiniMapCapture();
	void ClearMiniMapCaptures();
	FSceneMapView ResolveMiniMapView(APlayerController* InPlayer, FVector* OutLocation = nullptr) const;

protected:
	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, TObjectPtr<AMiniMapCapture>> MiniMapCaptures;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, TObjectPtr<UTextureRenderTarget2D>> MiniMapTargets;
	bool bRefreshingMiniMapCaptures = false;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	bool bMiniMapRotatable;

	UPROPERTY(EditAnywhere, Category = "MiniMap")
	EWorldMiniMapSource MiniMapSource;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "MiniMapSource == EWorldMiniMapSource::Widget"), Category = "MiniMap")
	TSubclassOf<UWidgetSceneMapBase> MapBackgroundWidgetClass;

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
	FSimpleMulticastDelegate OnMapPresentationChanged;

	UFUNCTION(BlueprintPure)
	AMiniMapCapture* GetMiniMapCapture(APlayerController* InPlayer) const;

	UFUNCTION(BlueprintPure)
	EWorldMiniMapSource GetMiniMapSource() const { return MiniMapSource; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapSource(EWorldMiniMapSource InMiniMapSource);

	UFUNCTION(BlueprintPure)
	TSubclassOf<UWidgetSceneMapBase> GetMapBackgroundWidgetClass() const { return MapBackgroundWidgetClass; }

	UFUNCTION(BlueprintCallable)
	void SetMapBackgroundWidgetClass(TSubclassOf<UWidgetSceneMapBase> InClass);

	UFUNCTION(BlueprintPure)
	EWorldMiniMapMode GetMiniMapMode() const { return MiniMapMode; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapMode(EWorldMiniMapMode InMiniMapMode);

	UFUNCTION(BlueprintPure)
	FTransform GetMiniMapPoint() const { return MiniMapPoint; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapPoint(const FTransform& InMiniMapPoint)
	{
		if (IsBusinessAdmissionOpen()) MiniMapPoint = InMiniMapPoint;
	}

	UFUNCTION(BlueprintPure)
	float GetMiniMapRange() const { return MiniMapRange; }

	UFUNCTION(BlueprintCallable)
	void SetMiniMapRange(float InMiniMapRange)
	{
		if (IsBusinessAdmissionOpen()) MiniMapRange = FMath::Clamp(InMiniMapRange, MiniMapMinRange, MiniMapMaxRange != -1.f ? MiniMapMaxRange : InMiniMapRange);
	}

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

	TMap<TWeakObjectPtr<ULocalPlayer>, FSceneMapView> PlayerWorldMapViews;

public:
	UFUNCTION(BlueprintPure)
	FVector2D GetWorldMapCenter() const { return WorldMapCenter; }

	UFUNCTION(BlueprintCallable)
	void SetWorldMapCenter(FVector2D InCenter)
	{
		if (IsBusinessAdmissionOpen()) WorldMapCenter = InCenter;
	}

	UFUNCTION(BlueprintPure)
	float GetWorldMapRange() const { return WorldMapRange; }

	UFUNCTION(BlueprintCallable)
	void SetWorldMapRange(float InRange)
	{
		if (IsBusinessAdmissionOpen()) WorldMapRange = FMath::Clamp(InRange, WorldMapMinRange, WorldMapMaxRange);
	}

	UFUNCTION(BlueprintPure)
	FSceneMapView GetMapView(ESceneMarkerChannel InChannel, APlayerController* InPlayer) const;

	UFUNCTION(BlueprintCallable)
	void SetPlayerWorldMapView(APlayerController* InPlayer, const FSceneMapView& InView);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Category = "SceneArea")
	TArray<FSceneArea> SceneAreas;

	TQueue<FSceneArea, EQueueMode::Mpsc> PendingSceneAreas;
	FCriticalSection PendingSceneAreasMutex;

	UPROPERTY(EditAnywhere, Category = "SceneArea")
	bool bDrawSceneArea;

	UPROPERTY(EditAnywhere, meta = (EditConditionHides, EditCondition = "bDrawSceneArea == true"), Category = "SceneArea")
	float SceneAreaHeight;

	TMap<ESceneAreaType, FSceneAreaResolver> SceneAreaResolvers;

public:
	FOnSceneAreaAdded OnSceneAreaAdded;
	FOnSceneAreasChanged OnSceneAreasChanged;

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
	TMap<FGuid, FGuid> PlayerTrackedMarkers;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ULocalPlayer>, FScenePlayerWorldMarkers> WorldMarkerWidgets;

	bool bWorldMarkerWidgetsDirty;
	bool bRefreshingWorldMarkerWidgets = false;
	uint32 WorldMarkerWidgetsGeneration = 0;

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
	FSceneMarkerView GetMarkerView(FGuid InPlayerID, FGuid InMarkerID, FVector InViewLocation, float InViewYaw = 0.f) const;

	UFUNCTION(BlueprintPure)
	TArray<FSceneMarkerView> GetMarkerViews(FGuid InPlayerID, ESceneMarkerChannel InChannel, FVector InViewLocation, float InViewYaw = 0.f, bool bFilterByDistance = true) const;

	UFUNCTION(BlueprintCallable)
	bool SetTrackedMarker(FGuid InPlayerID, FGuid InMarkerID);

	UFUNCTION(BlueprintPure)
	FGuid GetTrackedMarker(FGuid InPlayerID) const { return PlayerTrackedMarkers.FindRef(InPlayerID); }

	static FGuid GetPlayerMarkerOwnerId(APlayerController* InPlayer);

private:
	FSceneMarkerView ResolveMarker(FGuid InPlayerID, const FSceneMarker& InMarker, const FVector& InViewLocation, float InViewYaw) const;
	bool IsMarkerVisibleToPlayer(FGuid InPlayerID, const FSceneMarker& InMarker, ESceneMarkerChannel InChannel) const;
	void NotifySceneMarkersChanged();
	void RefreshWorldMarkerWidgets();
	void ClearWorldMarkerWidgets();
	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldTimer")
	UWorldTimer* WorldTimer;

public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldTimer* GetWorldTimer(TSubclassOf<UWorldTimer> InClass = nullptr) const;

	static int64 GetWorldTicks(UWorld* InWorld);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(EditAnywhere, Instanced, Category = "WorldWeather")
	UWorldWeather* WorldWeather;
	
public:
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UWorldWeather* GetWorldWeather(TSubclassOf<UWorldWeather> InClass = nullptr) const;

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(Transient)
	TMap<TObjectPtr<UDataLayerAsset>, TWeakObjectPtr<APlayerController>> DataLayerPlayerMappings;
	
	UPROPERTY(Transient)
	TMap<FName, TWeakObjectPtr<APlayerController>> LevelPlayerMappings;

	TMap<TWeakObjectPtr<AActor>, FSceneOwnerActorOverride> OwnerMappingOverrides;
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AActor>> OwnerMappingViewTargets;
	TSet<TWeakObjectPtr<AActor>> PendingOwnerMappingActors;
	TArray<FTimerHandle> SceneSequenceTimers;
	TWeakObjectPtr<UWorld> OwnerMappingWorld;
	FDelegateHandle OwnerMappingSpawnHandle;
	FDelegateHandle OwnerMappingLevelAddedHandle;
	FDelegateHandle OwnerMappingLevelRemovedHandle;
	bool bOwnerMappingFullRefresh = false;
	bool bReconcilingOwnerMappings = false;

	void BeginOwnerMappingMonitoring();
	void EndOwnerMappingMonitoring();
	void OnOwnerMappingActorSpawned(AActor* Actor);
	void OnOwnerMappingLevelChanged(ULevel* Level, UWorld* World);
	void OnOwnerMappingLevelRemoved(ULevel* Level, UWorld* World);
	void RequestOwnerMappingRefresh();
	void ApplyAllOwnerPlayerMappings();
	void ClearMarkersInternal(bool bIncludePersistent);
	void RefreshOwnerPlayerMappings();
	void ReconcileOwnerPlayerMappings();
	void ReleaseOwnerMappingActor(AActor* Actor, UWorld* World);
	void ReleaseOwnerMappingComponent(AActor* PreviousOwner, UPrimitiveComponent* Component,
		const FSceneOwnerComponentOverride& State, UWorld* World);
	
protected:
	UFUNCTION()
	void OnAsyncLoadLevels(UObject* InSender, const FEventAsyncLoadLevels& InEvent);

	UFUNCTION()
	void OnAsyncUnloadLevels(UObject* InSender, const FEventAsyncUnloadLevels& InEvent);

	UFUNCTION()
	void OnSetActorVisible(UObject* InSender, const FEventSetActorVisible& InEvent);
	
	UFUNCTION()
	void OnPlayLevelSequence(UObject* InSender, const FEventPlayLevelSequence& InEvent);
		
	UFUNCTION()
	void OnStopLevelSequence(UObject* InSender, const FEventStopLevelSequence& InEvent);

	UFUNCTION()
	void OnSetDataLayerRuntimeState(UObject* InSender, const FEventSetDataLayerRuntimeState& InEvent);
	
	UFUNCTION()
	void OnSetDataLayerOwnerPlayer(UObject* InSender, const FEventSetDataLayerOwnerPlayer& InEvent);
	
	UFUNCTION()
	void OnSetLevelOwnerPlayer(UObject* InSender, const FEventSetLevelOwnerPlayer& InEvent);

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
	// Explicit IDs assigned to deferred actors are reserved until their registration or release.
	TMap<FGuid, TWeakObjectPtr<AActor>> SceneActorReservations;
	TSet<TWeakObjectPtr<AActor>> PendingSceneActorRegistrations;
	TMap<TWeakObjectPtr<AActor>, FDeferredSceneActorRegistration> DeferredSceneActorRegistrations;
	uint64 SceneActorRegistrationSerial = 0;
	bool bRetryingSceneActorRegistrations = false;
	bool TryAddSceneActor(AActor* InActor, bool bReportConflict, const FGuid* ExpectedId = nullptr, bool bNotifyMarkers = true);
	void RetrySceneActorRegistrations();

	UPROPERTY(Transient)
	TSet<FGuid> DestroyedSceneActorIds;

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

	/** Atomically rekeys this World's registration; ApplyIdentity must only assign the native ID field. */
	bool ReassignSceneActorIdentity(AActor* InActor, const FGuid& OldId, const FGuid& NewId, TFunctionRef<void()> ApplyIdentity);
	bool QueueSceneActorRegistration(AActor* InActor, const FGuid& ExpectedId);

	void MarkSceneActorDestroyed(AActor* InActor);

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

private:
    int32 NextLevelOperationId = 0;
    bool bLevelOperationsClosing = false;
    float LevelFinishRemaining = 0.f;
    bool bWaitingLevelFinish = false;

    void StartLevelOperation(FAsyncLoadLevelTask& Task);
    void RefreshLevelOperationWidgets(int32 OperationId);
    void HandlePackageLoaded(int32 OperationId, TWeakObjectPtr<UWorld> OperationWorld, bool bSucceeded);
    void BeginStreamLevel(int32 OperationId);
    void CompleteLevelOperation(int32 OperationId, ESceneLevelLoadResult Result);
    void CancelLevelOperations();

    UFUNCTION()
    void OnStreamLevelFinished(int32 OperationId);
};
