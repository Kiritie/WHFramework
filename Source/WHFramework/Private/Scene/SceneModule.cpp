
#include "Scene/SceneModule.h"
#include "Common/CommonTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "Widget/WorldWidgetTypes.h"
#include "Common/LifecycleTypes.h"
#include "Scene/SceneMapTypes.h"
#include "Scene/SceneAreaTypes.h"
#include "Scene/SceneMarkerTypes.h"
#include "Scene/SceneStreamingTypes.h"
#include "Scene/SceneActorSaveTypes.h"
#include "Scene/WorldTimerTypes.h"
#include "Scene/WorldWeatherTypes.h"
#include "Scene/Camera/CameraStreamingBridge.h"

#include "Camera/CameraModuleStatics.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "Common/CommonModuleStatics.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Debug/DebugModuleTypes.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TargetPoint.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Scene/Event_AsyncLoadLevels.h"
#include "Event/Events/Scene/Event_AsyncLoadLevelFinished.h"
#include "Event/Events/Scene/Event_AsyncUnloadLevels.h"
#include "Event/Events/Scene/Event_AsyncUnloadLevelFinished.h"
#include "Event/Events/Scene/Event_PlayLevelSequence.h"
#include "Event/Events/Scene/Event_SetActorVisible.h"
#include "Event/Events/Scene/Event_SetDataLayerOwnerPlayer.h"
#include "Event/Events/Scene/Event_SetDataLayerRuntimeState.h"
#include "Event/Events/Scene/Event_SetLevelOwnerPlayer.h"
#include "Event/Events/Scene/Event_StopLevelSequence.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Main/MainModule.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/MathHelper.h"
#include "Runtime/LevelSequence/Public/LevelSequenceActor.h"
#include "Runtime/LevelSequence/Public/LevelSequencePlayer.h"
#include "Scene/Object/WorldTimer.h"
#include "Scene/Object/WorldWeather.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"
#include "Scene/Capture/MiniMapCapture.h"
#include "Scene/Widget/WidgetLoadingLevelPanel.h"
#include "Scene/Widget/WidgetSceneWorldMarker.h"
#include "Scene/Widget/WidgetWorldText.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

IMPLEMENTATION_MODULE(USceneModule)

USceneModule::USceneModule()
{
	SaveDataVersion = 2;
	ModuleName = FName("SceneModule");
	ModuleDisplayName = FText::FromString(TEXT("Scene Module"));
	SaveScope = ESaveScope::World;

	bModuleRequired = true;


	bSaveActorDatas = false;

	SeaLevel = 0.f;
	bMiniMapRotatable = false;
	MiniMapSource = EWorldMiniMapSource::SceneCapture;
	MapBackgroundWidgetClass = nullptr;

	MiniMapMode = EWorldMiniMapMode::None;
	MiniMapPoint = FTransform::Identity;
	MiniMapRange = 512.f;
	MiniMapMinRange = 128.f;
	MiniMapMaxRange = -1.f;

	WorldMapCenter = FVector2D::ZeroVector;
	WorldMapRange = 4096.f;
	WorldMapMinRange = 512.f;
	WorldMapMaxRange = 3000000.f;

	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> MiniMapTexFinder(TEXT("/Script/Engine.TextureRenderTarget2D'/WHFramework/Scene/Textures/Render/RT_MiniMap_Default.RT_MiniMap_Default'"));
	if(MiniMapTexFinder.Succeeded())
	{
		MiniMapTexture = MiniMapTexFinder.Object;
	}

	SceneAreas = TArray<FSceneArea>();
	Markers = TMap<FGuid, FSceneMarker>();
	WorldMarkerWidgets.Reset();
	bWorldMarkerWidgetsDirty = true;
	bDrawSceneArea = false;
	SceneAreaHeight = 1000.f;

	WorldTimer = nullptr;
	WorldWeather = nullptr;

	SceneActors = TArray<AActor*>();
	SceneActorMap = TMap<FGuid, AActor*>();

	TargetPoints = TMap<FName, ATargetPoint*>();

	ScenePoints = TMap<FName, USceneComponent*>();

	PhysicsVolumes = TMap<FName, APhysicsVolumeBase*>();

	DefaultPhysicsVolumes = TArray<FPhysicsVolumeData>();

	FPhysicsVolumeData WaterPhysicsVolume;
	WaterPhysicsVolume.Name = FName("Water");
	WaterPhysicsVolume.Priority = 1.f;
	WaterPhysicsVolume.bWaterVolume = true;
	WaterPhysicsVolume.PhysicsVolumeClass = APhysicsVolumeBase::StaticClass();
	DefaultPhysicsVolumes.Add(WaterPhysicsVolume);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OutlineMatFinder(TEXT("Material'/WHFramework/Scene/Materials/M_PostHigtLight.M_PostHigtLight'"));
	if(OutlineMatFinder.Succeeded())
	{
		OutlineMat = OutlineMatFinder.Object;
	}

	OutlineColor = FLinearColor(0.03f, 0.7f, 1.f);
	OutlineMatInst = nullptr;

}

USceneModule::~USceneModule()
{
}

#if WITH_EDITOR
void USceneModule::OnGenerate()
{
	Super::OnGenerate();
}

void USceneModule::OnDestroy()
{
	bSceneRuntimeClosing = true;
	EndOwnerMappingMonitoring();
	ClearCameraStreamingBridges();
	Super::OnDestroy();


	ClearMiniMapCaptures();
}
#endif

void USceneModule::OnInitialize()
{
	bSceneRuntimeClosing = false;
	SaveDataVersion = 2;
	Super::OnInitialize();

	RefreshMiniMapCapture();
	IDebuggerInterface::Register();
	RefreshCameraStreamingBridges();

	UEventModuleStatics::SubscribeEvent<FEventAsyncLoadLevels>(this, &ThisClass::OnAsyncLoadLevels);
	UEventModuleStatics::SubscribeEvent<FEventAsyncUnloadLevels>(this, &ThisClass::OnAsyncUnloadLevels);
	UEventModuleStatics::SubscribeEvent<FEventSetActorVisible>(this, &ThisClass::OnSetActorVisible);
	UEventModuleStatics::SubscribeEvent<FEventPlayLevelSequence>(this, &ThisClass::OnPlayLevelSequence);
	UEventModuleStatics::SubscribeEvent<FEventStopLevelSequence>(this, &ThisClass::OnStopLevelSequence);
	UEventModuleStatics::SubscribeEvent<FEventSetDataLayerRuntimeState>(this, &ThisClass::OnSetDataLayerRuntimeState);
	UEventModuleStatics::SubscribeEvent<FEventSetDataLayerOwnerPlayer>(this, &ThisClass::OnSetDataLayerOwnerPlayer);
	UEventModuleStatics::SubscribeEvent<FEventSetLevelOwnerPlayer>(this, &ThisClass::OnSetLevelOwnerPlayer);
	BeginOwnerMappingMonitoring();

	if(WorldTimer)
	{
		WorldTimer->OnInitialize();
	}
	if(WorldWeather)
	{
		WorldWeather->OnInitialize();
	}

	for(auto Iter : SceneActors)
	{
		AddSceneActor(Iter);
	}

	for(auto Iter : DefaultPhysicsVolumes)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if(APhysicsVolumeBase* PhysicsVolume = GetWorld()->SpawnActor<APhysicsVolumeBase>(ActorSpawnParameters))
		{
			PhysicsVolume->Initialize(Iter);
			PhysicsVolumes.Add(Iter.Name, PhysicsVolume);
		}
	}

	OutlineMatInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, OutlineMat);

	APostProcessVolume* UnboundPostProcessVolume = nullptr;

	for (IInterface_PostProcessVolume* PostProcessVolumeInterface : GetWorld()->PostProcessVolumes)
	{
		if (PostProcessVolumeInterface)
		{
			if (APostProcessVolume* PostProcessVolume = Cast<APostProcessVolume>(PostProcessVolumeInterface))
			{
				if (PostProcessVolume->bUnbound)
				{
					UnboundPostProcessVolume = PostProcessVolume;
					break;
				}
			}
		}
	}

	if (!UnboundPostProcessVolume)
	{
		APostProcessVolume* PostProcessVolume = GetWorld()->SpawnActor<APostProcessVolume>();

		if (PostProcessVolume)
		{
			PostProcessVolume->bUnbound = true;
			UnboundPostProcessVolume = PostProcessVolume;
		}
	}

	if (UnboundPostProcessVolume)
	{
		bool bNeedAddMat = true;
		// 查找是否描边材质已经添加
		for (int32 i = 0; i < UnboundPostProcessVolume->Settings.WeightedBlendables.Array.Num(); ++i)
		{
			if (UnboundPostProcessVolume->Settings.WeightedBlendables.Array[i].Object == OutlineMat)
			{
				if (OutlineMatInst)
				{
					UnboundPostProcessVolume->Settings.WeightedBlendables.Array[i].Object = OutlineMatInst;
				}
				bNeedAddMat = false;
				break;
			}
		}
		if (bNeedAddMat)
		{
			UnboundPostProcessVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, OutlineMatInst ? OutlineMatInst : OutlineMat));
		}
	}

	SetOutlineColor(OutlineColor);
}

void USceneModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		UpdateMiniMapCapture();
		if(WorldTimer)
    	{
    		WorldTimer->OnPreparatory();
    	}
    	if(WorldWeather)
    	{
    		WorldWeather->OnPreparatory();
    	}
	}
}

void USceneModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	if (!IsBusinessAdmissionOpen()) return;
	Super::OnRefresh(DeltaSeconds, bInEditor);
	if (!IsBusinessAdmissionOpen()) return;

	if(bInEditor) return;

	FSceneArea PendingSceneArea;
	while(PendingSceneAreas.Dequeue(PendingSceneArea))
	{
		AddSceneArea(PendingSceneArea);
	}


	if(WorldTimer)
	{
		WorldTimer->OnRefresh(DeltaSeconds);
	}

	if(WorldWeather)
	{
		WorldWeather->OnRefresh(DeltaSeconds);
	}


}

void USceneModule::OnControlRefresh(float RealDeltaSeconds)
{
	if (!IsBusinessAdmissionOpen()) return;
    Super::OnControlRefresh(RealDeltaSeconds);
	for (auto It = SceneActorReservations.CreateIterator(); It; ++It) if (!It.Value().IsValid()) It.RemoveCurrent();
	RetrySceneActorRegistrations();
	RefreshOwnerPlayerMappings();
	RefreshCameraStreamingBridges();
	UpdateMiniMapCapture();
	RefreshWorldMarkerWidgets();
    if(bWaitingLevelFinish && !AsyncLoadLevelQueue.IsEmpty())
    {
        LevelFinishRemaining -= RealDeltaSeconds;
        if(LevelFinishRemaining <= 0.f)
        {
            bWaitingLevelFinish = false;
            CompleteLevelOperation(AsyncLoadLevelQueue[0].OperationId, ESceneLevelLoadResult::Succeeded);
        }
    }
    if(!bLevelOperationsClosing && !AsyncLoadLevelQueue.IsEmpty() && !AsyncLoadLevelQueue[0].bLoading)
    {
        StartLevelOperation(AsyncLoadLevelQueue[0]);
    }
    if (!bLevelOperationsClosing && !AsyncLoadLevelQueue.IsEmpty() && AsyncLoadLevelQueue[0].bLoading)
    {
        RefreshLevelOperationWidgets(AsyncLoadLevelQueue[0].OperationId);
    }
}

void USceneModule::OnPause()
{
	Super::OnPause();

	if(WorldTimer)
	{
		WorldTimer->OnPause();
	}
	if(WorldWeather)
	{
		WorldWeather->OnPause();
	}
}

void USceneModule::OnUnPause()
{
	Super::OnUnPause();

	if(WorldTimer)
	{
		WorldTimer->OnUnPause();
	}
	if(WorldWeather)
	{
		WorldWeather->OnUnPause();
	}
}

void USceneModule::OnTermination(EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Primary))
	{
		bSceneRuntimeClosing = true;
		EndOwnerMappingMonitoring();
	}
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		if (UWorld* World = GetWorld())
			for (FTimerHandle& Timer : SceneSequenceTimers) World->GetTimerManager().ClearTimer(Timer);
		SceneSequenceTimers.Reset();
		DeferredSceneActorRegistrations.Reset();
		SceneActorReservations.Reset();
        CancelLevelOperations();
		IDebuggerInterface::UnRegister();
		ClearCameraStreamingBridges();
		ClearMiniMapCaptures();

		{
			FScopeLock Lock(&PendingSceneAreasMutex);
			FSceneArea Area;
			while(PendingSceneAreas.Dequeue(Area)) { }
		}
		ClearWorldMarkerWidgets();
		ClearMarkersInternal(true);
	}
}

bool USceneModule::LoadSceneSaveData(const FParameter& InData, EPhase InPhase)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (bSceneLoadInProgress || bSceneRuntimeClosing || !InData.Is<FSceneModuleSaveData>()) return false;
	TGuardValue<bool> Loading(bSceneLoadInProgress, true);
	bLastSceneLoadSucceeded = false;
	return LoadSaveData(InData, InPhase) && bLastSceneLoadSucceeded;
}

void USceneModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!IsBusinessAdmissionOpen())
	{
		MarkLoadDataFailed();
		return;
	}
	bLastSceneLoadSucceeded = false;
	const FSceneModuleSaveData* Data = InSaveData.GetPtr<FSceneModuleSaveData>();
	if (!Data || bSceneRuntimeClosing) return;
	const FSceneModuleSaveData& SaveData = *Data;

	if(PHASEC(InPhase, EPhase::All))
	{
		if(PHASEC(InPhase, EPhase::Primary) && SaveData.IsSaved())
		{
			MiniMapRange = SaveData.MiniMapRange;
			WorldMapCenter = SaveData.WorldMapCenter;
			WorldMapRange = SaveData.WorldMapRange;
			SceneAreas = SaveData.SceneAreas;
			Markers.Reset();
			for(const FSceneMarker& Marker : SaveData.Markers)
			{
				if(Marker.MarkerID.IsValid()) Markers.Add(Marker.MarkerID, Marker);
			}
			PlayerTrackedMarkers = SaveData.PlayerTrackedMarkers;
			NotifySceneMarkersChanged();
		}

		if(WorldTimer && WorldTimer->IsAutoSave())
		{
			if (!WorldTimer->LoadSaveData(FParameter(SaveData.TimerData), InPhase)) return;
		}

		if(WorldWeather && WorldWeather->IsAutoSave())
		{
			if (!WorldWeather->LoadSaveData(FParameter(SaveData.WeatherData), InPhase)) return;
		}

		if(WorldTimer)
		{
			WorldTimer->OnRefresh(0.f);
		}
		if(WorldWeather)
		{
			WorldWeather->OnRefresh(0.f);
		}

		if(bSaveActorDatas)
		{
			if (PHASEC(InPhase, EPhase::Primary)) DestroyedSceneActorIds.Reset();
			for(const FSceneActorSaveRecord& Record : SaveData.ActorSaveRecords)
			{
				if (!Record.ActorId.IsValid()) return;
				AActor* Actor = GetSceneActor(Record.ActorId.ToString(), nullptr, false);
				if (Record.bDestroyed)
				{
					if (PHASEC(InPhase, EPhase::Primary))
					{
						if (Actor && !Actor->Destroy()) return;
						DestroyedSceneActorIds.Add(Record.ActorId);
					}
					continue;
				}
				if(PHASEC(InPhase, EPhase::Primary))
				{
					if(!Actor && Record.bRuntimeSpawned && !Record.ActorClass.IsNull())
					{
						UClass* ActorClass = Record.ActorClass.TryLoadClass<AActor>();
						if(ActorClass)
						{
							Actor = GetWorld()->SpawnActorDeferred<AActor>(ActorClass, Record.Transform);
							if(Actor && Actor->Implements<USceneActorInterface>())
							{
								if (!ISceneActorInterface::Execute_SetActorID(Actor, Record.ActorId)) { Actor->Destroy(); return; }
								UGameplayStatics::FinishSpawningActor(Actor, Record.Transform);
								if (!AddSceneActor(Actor) && GetSceneActor(Record.ActorId.ToString(), nullptr, false) != Actor) return;
							}
							else if (Actor)
							{
								Actor->Destroy();
								return;
							}
						}
					}
				}
				if (!::IsValid(Actor)) return;
				Actor->SetActorTransform(Record.Transform);
				ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Actor);
				if (!Agent || !Agent->LoadSaveData(Record.Data, InPhase)) return;
			}
		}
	}
	bLastSceneLoadSucceeded = true;
}

FParameter USceneModule::ToData()
{
	if (bSceneRuntimeClosing) return FParameter();
	FSceneModuleSaveData SaveData;
	SaveData.MiniMapRange = MiniMapRange;
	SaveData.WorldMapCenter = WorldMapCenter;
	SaveData.WorldMapRange = WorldMapRange;
	SaveData.PlayerTrackedMarkers = PlayerTrackedMarkers;
	SaveData.SceneAreas = SceneAreas;
	for (const auto& Iter : Markers)
	{
		if (Iter.Value.bPersistent) SaveData.Markers.Add(Iter.Value);
	}
	if (WorldTimer && WorldTimer->IsAutoSave())
	{
		const FParameter Data = WorldTimer->GetSaveData(true);
		const FWorldTimerSaveData* Timer = Data.GetPtr<FWorldTimerSaveData>();
		if (!Timer)
		{
			UE_LOG(LogTemp, Error, TEXT("Scene capture failed for the world timer"));
			return FParameter();
		}
		SaveData.TimerData = *Timer;
	}
	if (WorldWeather && WorldWeather->IsAutoSave())
	{
		const FParameter Data = WorldWeather->GetSaveData(true);
		const FWorldWeatherSaveData* Weather = Data.GetPtr<FWorldWeatherSaveData>();
		if (!Weather)
		{
			UE_LOG(LogTemp, Error, TEXT("Scene capture failed for the world weather"));
			return FParameter();
		}
		SaveData.WeatherData = *Weather;
	}
	if (bSaveActorDatas)
	{
		const TMap<FGuid, AActor*> CapturedActors = SceneActorMap;
		for (const TPair<FGuid, AActor*>& Iter : CapturedActors)
		{
			AActor* Actor = Iter.Value;
			if (!::IsValid(Actor))
			{
				UE_LOG(LogTemp, Error, TEXT("Scene capture lost a registered actor"));
				return FParameter();
			}
			if (ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Actor))
			{
				FSceneActorSaveRecord Record;
				Record.ActorId = Iter.Key;
				Record.ActorClass = FSoftClassPath(Actor->GetClass());
				Record.Transform = Actor->GetActorTransform();
				Record.bRuntimeSpawned = !Actor->HasAnyFlags(RF_WasLoaded);
				Record.Data = Agent->GetSaveData(true);
				if (!Record.Data.HasValue())
				{
					UE_LOG(LogTemp, Error, TEXT("Scene capture failed for %s; actor was not omitted"), *Iter.Key.ToString());
					return FParameter();
				}
				if (Actor->Implements<UPrimaryEntityInterface>())
				{
					Record.AssetId = IPrimaryEntityInterface::Execute_GetAssetID(Actor);
				}
				SaveData.ActorSaveRecords.Add(MoveTemp(Record));
			}
		}
		for (const FGuid& ActorId : DestroyedSceneActorIds)
		{
			FSceneActorSaveRecord Record;
			Record.ActorId = ActorId;
			Record.bDestroyed = true;
			SaveData.ActorSaveRecords.Add(MoveTemp(Record));
		}
	}
	return FParameter(MoveTemp(SaveData));
}

FString USceneModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

void USceneModule::OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
	if(bDrawSceneArea)
	{
		for(auto& Iter1 : SceneAreas)
		{
			switch(Iter1.AreaShape)
			{
				case ESceneAreaShape::Box:
				{
					UKismetSystemLibrary::DrawDebugBox(this, FVector(Iter1.AreaCenter.X, Iter1.AreaCenter.Y, SceneAreaHeight * 0.5f), FVector(Iter1.AreaRadius.X, Iter1.AreaRadius.Y, SceneAreaHeight * 0.5f), FLinearColor::Red);
					break;
				}
				case ESceneAreaShape::Ellipse:
				{
					UKismetSystemLibrary::DrawDebugCylinder(this, FVector(Iter1.AreaCenter.X, Iter1.AreaCenter.Y, 0.f), FVector(Iter1.AreaCenter.X, Iter1.AreaCenter.Y, SceneAreaHeight), Iter1.AreaRadius.GetMax(), 12, FLinearColor::Red);
					break;
				}
				case ESceneAreaShape::Polygon:
				{
					for(auto& Iter2 : Iter1.AreaPoints)
					{
						UKismetSystemLibrary::DrawDebugLine(this, FVector(Iter2.X, Iter2.Y, 0.f), FVector(Iter2.X, Iter2.Y, SceneAreaHeight), FLinearColor::Red);
					}
					break;
				}
			}
		}
	}
}

#if WITH_EDITOR
void USceneModule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapSource) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapTexture) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapMode))
		{
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapTexture)) ClearMiniMapCaptures();
			RefreshMiniMapCapture();
		}
		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapSource) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapTexture) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MapBackgroundWidgetClass) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapMode))
		{
			OnMapPresentationChanged.Broadcast();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

float USceneModule::GetAltitude(APlayerController* InPlayer, bool bUnsigned) const
{
	if (!InPlayer || InPlayer->GetWorld() != GetWorld() || !InPlayer->IsLocalController()) return 0.f;
	FVector Location;
	FRotator Rotation;
	InPlayer->GetPlayerViewPoint(Location, Rotation);
	const float ReturnValue = Location.Z - SeaLevel;
	return bUnsigned ? FMath::Max(ReturnValue, 0.f) : ReturnValue;
}

bool USceneModule::HasSceneArea(const FName InName) const
{
	for(auto& Iter : SceneAreas)
	{
		if(Iter.AreaName == InName)
		{
			return true;
		}
	}
	return false;
}

FSceneArea USceneModule::GetSceneArea(const FName InName) const
{
	for(auto& Iter : SceneAreas)
	{
		if(Iter.AreaName == InName)
		{
			return Iter;
		}
	}
	return FSceneArea();
}

FSceneArea USceneModule::GetSceneAreaByPoint(const FVector2D& InPoint) const
{
	const FSceneArea* BestArea = nullptr;
	double BestAreaSize = TNumericLimits<double>::Max();
	for(const FSceneArea& Iter : SceneAreas)
	{
		bool bContains = false;
		double AreaSize = TNumericLimits<double>::Max();
		switch(Iter.AreaShape)
		{
			case ESceneAreaShape::Box:
			{
				const FVector2D Min = Iter.AreaCenter - Iter.AreaRadius;
				const FVector2D Max = Iter.AreaCenter + Iter.AreaRadius;
				bContains = InPoint.X >= Min.X && InPoint.X < Max.X && InPoint.Y >= Min.Y && InPoint.Y < Max.Y;
				AreaSize = Iter.AreaRadius.X * Iter.AreaRadius.Y * 4.0;
				break;
			}
			case ESceneAreaShape::Ellipse:
			{
				bContains = FMathHelper::IsPointInEllipse2D(InPoint, Iter.AreaCenter, Iter.AreaRadius);
				AreaSize = Iter.AreaRadius.X * Iter.AreaRadius.Y * PI;
				break;
			}
			case ESceneAreaShape::Polygon:
			{
				bContains = FMathHelper::IsPointInPolygon2D(InPoint, Iter.AreaPoints);
				if(!Iter.AreaPoints.IsEmpty())
				{
					const FBox2D Bounds(Iter.AreaPoints);
					AreaSize = Bounds.GetArea();
				}
				break;
			}
		}
		if(bContains && AreaSize < BestAreaSize)
		{
			BestArea = &Iter;
			BestAreaSize = AreaSize;
		}
	}
	if(!BestArea) return FSceneArea();
	if(const FSceneAreaResolver* Resolver = SceneAreaResolvers.Find(BestArea->AreaType))
	{
		if(Resolver->IsBound()) return Resolver->Execute(*BestArea, InPoint);
	}
	return *BestArea;
}

void USceneModule::RegisterSceneAreaResolver(ESceneAreaType InType, const FSceneAreaResolver& InResolver)
{
	if (!IsBusinessAdmissionOpen()) return;
	SceneAreaResolvers.Add(InType, InResolver);
}

void USceneModule::UnregisterSceneAreaResolver(ESceneAreaType InType)
{
	SceneAreaResolvers.Remove(InType);
}

void USceneModule::AddSceneArea(const FSceneArea& InArea, bool bThreadSafe)
{
	if(bThreadSafe)
	{
		FScopeLock Lock(&PendingSceneAreasMutex);
		if (!IsBusinessAdmissionOpen()) return;
		PendingSceneAreas.Enqueue(InArea);
		return;
	}
	if (!IsBusinessAdmissionOpen()) return;

	FSceneArea Area = InArea;
	if(Area.AreaName.IsNone())
	{
		Area.AreaName = *FString::Printf(TEXT("Area_%d"), SceneAreas.Num());
	}
	if(!HasSceneArea(Area.AreaName))
	{
		SceneAreas.Add(Area);
		OnSceneAreaAdded.Broadcast(Area);
		if (!IsBusinessAdmissionOpen()) return;
		OnSceneAreasChanged.Broadcast();
	}
}

void USceneModule::RemoveSceneArea(const FName InName)
{
	if (!IsBusinessAdmissionOpen()) return;
	for(int32 Index = 0; Index < SceneAreas.Num(); ++Index)
	{
		if(SceneAreas[Index].AreaName == InName)
		{
			SceneAreas.RemoveAt(Index);
			OnSceneAreasChanged.Broadcast();
			return;
		}
	}
}

void USceneModule::ClearSceneArea()
{
	if (!IsBusinessAdmissionOpen()) return;
	const bool bHadSceneAreas = !SceneAreas.IsEmpty();
	SceneAreas.Empty();

	{
		FScopeLock Lock(&PendingSceneAreasMutex);
		FSceneArea Area;
		while(PendingSceneAreas.Dequeue(Area)) { }
	}
	if(bHadSceneAreas) OnSceneAreasChanged.Broadcast();
}

FGuid USceneModule::AddMarker(const FSceneMarker& InMarker)
{
	if (!IsBusinessAdmissionOpen()) return FGuid();
	FSceneMarker Marker = InMarker;
	if(!Marker.MarkerID.IsValid()) Marker.MarkerID = FGuid::NewGuid();
	if(Markers.Contains(Marker.MarkerID)) return FGuid();
	Markers.Add(Marker.MarkerID, Marker);
	NotifySceneMarkersChanged();
	return Marker.MarkerID;
}

bool USceneModule::UpdateMarker(const FSceneMarker& InMarker)
{
	if (!IsBusinessAdmissionOpen()) return false;
	FSceneMarker* Existing = InMarker.MarkerID.IsValid() ? Markers.Find(InMarker.MarkerID) : nullptr;
	if(!Existing) return false;
	if(Existing->MarkerTag == InMarker.MarkerTag && Existing->DisplayName.EqualTo(InMarker.DisplayName) &&
		Existing->Symbol.EqualTo(InMarker.Symbol) && Existing->Badge.EqualTo(InMarker.Badge) && Existing->Description.EqualTo(InMarker.Description) && Existing->Icon == InMarker.Icon &&
		Existing->Color.Equals(InMarker.Color) && Existing->Location.Equals(InMarker.Location) && Existing->Offset.Equals(InMarker.Offset) &&
		Existing->ActorID == InMarker.ActorID && Existing->AreaName == InMarker.AreaName && Existing->Channels == InMarker.Channels &&
		Existing->OwnerPlayerId == InMarker.OwnerPlayerId && Existing->bNavigationOnlyWhenTracked == InMarker.bNavigationOnlyWhenTracked &&
		Existing->Priority == InMarker.Priority && FMath::IsNearlyEqual(Existing->MinDistance, InMarker.MinDistance) &&
		FMath::IsNearlyEqual(Existing->MaxDistance, InMarker.MaxDistance) && Existing->bPersistent == InMarker.bPersistent) return true;
	*Existing = InMarker;
	NotifySceneMarkersChanged();
	return true;
}

bool USceneModule::RemoveMarker(FGuid InMarkerID)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if(!Markers.Remove(InMarkerID)) return false;
	for (auto It = PlayerTrackedMarkers.CreateIterator(); It; ++It)
	{
		if (It.Value() == InMarkerID) It.RemoveCurrent();
	}
	NotifySceneMarkersChanged();
	return true;
}

void USceneModule::ClearMarkers(bool bIncludePersistent)
{
	if (!IsBusinessAdmissionOpen()) return;
	ClearMarkersInternal(bIncludePersistent);
}

void USceneModule::ClearMarkersInternal(bool bIncludePersistent)
{
	bool bChanged = false;
	for(auto Iter = Markers.CreateIterator(); Iter; ++Iter)
	{
		if(bIncludePersistent || !Iter.Value().bPersistent)
		{
			for (auto Tracked = PlayerTrackedMarkers.CreateIterator(); Tracked; ++Tracked)
			{
				if (Tracked.Value() == Iter.Key()) Tracked.RemoveCurrent();
			}
			Iter.RemoveCurrent();
			bChanged = true;
		}
	}
	if (bIncludePersistent && !PlayerTrackedMarkers.IsEmpty())
	{
		PlayerTrackedMarkers.Reset();
		bChanged = true;
	}
	if(bChanged && IsBusinessAdmissionOpen()) NotifySceneMarkersChanged();
}

FSceneMarker USceneModule::GetMarker(FGuid InMarkerID) const
{
	if(const FSceneMarker* Marker = Markers.Find(InMarkerID)) return *Marker;
	return FSceneMarker();
}

bool USceneModule::SetTrackedMarker(FGuid InPlayerID, FGuid InMarkerID)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!InPlayerID.IsValid()) return false;
	if(InMarkerID.IsValid() && !Markers.Contains(InMarkerID)) return false;
	if (const FSceneMarker* Marker = Markers.Find(InMarkerID); Marker && Marker->OwnerPlayerId.IsValid() && Marker->OwnerPlayerId != InPlayerID) return false;
	if(GetTrackedMarker(InPlayerID) == InMarkerID) return true;
	if (InMarkerID.IsValid()) PlayerTrackedMarkers.Add(InPlayerID, InMarkerID);
	else PlayerTrackedMarkers.Remove(InPlayerID);
	NotifySceneMarkersChanged();
	return true;
}

UWorldTimer* USceneModule::GetWorldTimer(TSubclassOf<UWorldTimer> InClass) const
{
	return GetDeterminesOutputObject(WorldTimer, InClass);
}

int64 USceneModule::GetWorldTicks(UWorld* InWorld)
{
	const USceneModule* Scene = GetPtr(InWorld);
	const UWorldTimer* Timer = Scene ? Scene->GetWorldTimer() : nullptr;
	return Timer ? Timer->GetDateTime().GetTicks() : 0;
}

UWorldWeather* USceneModule::GetWorldWeather(TSubclassOf<UWorldWeather> InClass) const
{
	return GetDeterminesOutputObject(WorldWeather, InClass);
}

void USceneModule::OnAsyncLoadLevels(UObject* InSender, const FEventAsyncLoadLevels& InEvent)
{
	if (!IsBusinessAdmissionOpen()) return;
	for(auto& Iter : InEvent.SoftLevelPaths)
	{
		FOnAsyncLoadLevelFinished OnAsyncLoadLevelFinished;
		if(Iter.LevelObjectPtr)
		{
			AsyncLoadLevelByObjectPtr(Iter.LevelObjectPtr, OnAsyncLoadLevelFinished, InEvent.FinishDelayTime, InEvent.bCreateLoadingWidget);
		}
		else
		{
			AsyncLoadLevel(Iter.LevelPath, OnAsyncLoadLevelFinished, InEvent.FinishDelayTime, InEvent.bCreateLoadingWidget);
		}
	}
}

void USceneModule::OnAsyncUnloadLevels(UObject* InSender, const FEventAsyncUnloadLevels& InEvent)
{
	if (!IsBusinessAdmissionOpen()) return;
	for(auto& Iter : InEvent.SoftLevelPaths)
	{
		FOnAsyncLoadLevelFinished OnAsyncUnloadLevelFinished;
		if(Iter.LevelObjectPtr)
		{
			AsyncUnloadLevelByObjectPtr(Iter.LevelObjectPtr, OnAsyncUnloadLevelFinished, InEvent.FinishDelayTime, InEvent.bCreateLoadingWidget);
		}
		else
		{
			AsyncUnloadLevel(Iter.LevelPath, OnAsyncUnloadLevelFinished, InEvent.FinishDelayTime, InEvent.bCreateLoadingWidget);
		}
	}
}

void USceneModule::OnSetActorVisible(UObject* InSender, const FEventSetActorVisible& InEvent)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(AActor* Actor = InEvent.ActorPath.LoadSynchronous())
	{
		if (Actor->GetRootComponent())
			Actor->GetRootComponent()->SetVisibility(InEvent.bVisible, true);
	}
}

void USceneModule::OnPlayLevelSequence(UObject* InSender, const FEventPlayLevelSequence& InEvent)
{
	if (!IsBusinessAdmissionOpen()) return;
	UWorld* World = GetWorld();
	if (!World) return;
	if(ALevelSequenceActor* Actor = InEvent.LevelSequence.LoadSynchronous())
	{
		const TWeakObjectPtr<USceneModule> Owner(this);
		const TWeakObjectPtr<ALevelSequenceActor> SequenceActor(Actor);
		auto PlaySequence = [Owner, SequenceActor, InEvent]()
		{
			USceneModule* Scene = Owner.Get();
			ALevelSequenceActor* Target = SequenceActor.Get();
			if (!Scene || !Scene->IsBusinessAdmissionOpen() || !Target) return;
			ULevelSequencePlayer* Player = Target->GetSequencePlayer();
			if (!Player) return;
			if(!InEvent.bReverse)
			{
				Player->Play();
			}
			else
			{
				Player->PlayReverse();
			}
		};
		if(InEvent.Delay > 0.f)
		{
			const TSharedRef<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();
			World->GetTimerManager().SetTimer(*TimerHandle, [Owner, TimerHandle, PlaySequence]()
			{
				if (USceneModule* Scene = Owner.Get()) Scene->SceneSequenceTimers.Remove(*TimerHandle);
				PlaySequence();
			}, InEvent.Delay, false);
			SceneSequenceTimers.Add(*TimerHandle);
		}
		else
		{
			PlaySequence();
		}
	}
}

void USceneModule::OnStopLevelSequence(UObject* InSender, const FEventStopLevelSequence& InEvent)
{
	if (!IsBusinessAdmissionOpen()) return;
	UWorld* World = GetWorld();
	if (!World) return;
	if(ALevelSequenceActor* Actor = InEvent.LevelSequence.LoadSynchronous())
	{
		const TWeakObjectPtr<USceneModule> Owner(this);
		const TWeakObjectPtr<ALevelSequenceActor> SequenceActor(Actor);
		auto StopSequence = [Owner, SequenceActor, InEvent]()
		{
			USceneModule* Scene = Owner.Get();
			ALevelSequenceActor* Target = SequenceActor.Get();
			if (!Scene || !Scene->IsBusinessAdmissionOpen() || !Target) return;
			ULevelSequencePlayer* Player = Target->GetSequencePlayer();
			if (!Player) return;
			if(!InEvent.bKeepState)
			{
				Player->Stop();
			}
			else
			{
				Player->StopAtCurrentTime();
			}
		};
		if(InEvent.Delay > 0.f)
		{
			const TSharedRef<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();
			World->GetTimerManager().SetTimer(*TimerHandle, [Owner, TimerHandle, StopSequence]()
			{
				if (USceneModule* Scene = Owner.Get()) Scene->SceneSequenceTimers.Remove(*TimerHandle);
				StopSequence();
			}, InEvent.Delay, false);
			SceneSequenceTimers.Add(*TimerHandle);
		}
		else
		{
			StopSequence();
		}
	}
}

bool USceneModule::HasTraceMapping(const FName InName, bool bEnsured) const
{
	if(TraceMappings.Contains(InName)) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No trace mapping, trace name: %s"), *InName.ToString()), EDC_Scene, EDV_Error);
	return false;
}

FTraceMapping USceneModule::GetTraceMapping(const FName InName, bool bEnsured) const
{
	if(HasTraceMapping(InName, bEnsured))
	{
		return TraceMappings[InName];
	}
	return FTraceMapping();
}

void USceneModule::AddTraceMapping(const FName InName, ECollisionChannel InTraceChannel)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!TraceMappings.Contains(InName))
	{
		TraceMappings.Add(InName, InTraceChannel);
	}
}

void USceneModule::RemoveTraceMapping(const FName InName)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(TraceMappings.Contains(InName))
	{
		TraceMappings.Remove(InName);
	}
}

bool USceneModule::HasSceneActor(const FString& InID, bool bEnsured) const
{
	FGuid Id;
	AActor* const* Entry = FGuid::Parse(InID, Id) && Id.IsValid() ? SceneActorMap.Find(Id) : nullptr;
	if (Entry && ::IsValid(*Entry) && (*Entry)->GetWorld() == GetWorld()) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No scene actor, actor id: %s"), *InID), EDC_Scene, EDV_Error);
	return false;
}

AActor* USceneModule::GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if (!HasSceneActor(InID, bEnsured)) return nullptr;
	FGuid Id;
	if (!FGuid::Parse(InID, Id)) return nullptr;
	AActor* const* Actor = SceneActorMap.Find(Id);
	return GetDeterminesOutputObject(Actor ? *Actor : nullptr, InClass);
}

bool USceneModule::AddSceneActor(AActor* InActor)
{
	if (!IsBusinessAdmissionOpen()) return false;
	return TryAddSceneActor(InActor, true);
}

bool USceneModule::TryAddSceneActor(AActor* InActor, bool bReportConflict, const FGuid* ExpectedId, bool bNotifyMarkers)
{
	if (!IsBusinessAdmissionOpen()) return false;
	UWorld* World = GetWorld();
	if (bSceneRuntimeClosing || !::IsValid(InActor) || !World || World->bIsTearingDown || InActor->GetWorld() != World ||
		!InActor->Implements<USceneActorInterface>()) return false;
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	if (PendingSceneActorRegistrations.Contains(ActorKey)) return false;
	PendingSceneActorRegistrations.Add(ActorKey);
	ON_SCOPE_EXIT { PendingSceneActorRegistrations.Remove(ActorKey); };
	const FGuid Id = ISceneActorInterface::Execute_GetActorID(InActor);
	if (ExpectedId && Id != *ExpectedId) return false;
	if (const FDeferredSceneActorRegistration* Deferred = DeferredSceneActorRegistrations.Find(ActorKey); Deferred && Deferred->ExpectedId != Id) return false;
	if (AActor* const* Existing = SceneActorMap.Find(Id); Existing && *Existing == InActor) return true;
	if (TWeakObjectPtr<AActor>* Reserved = SceneActorReservations.Find(Id); Reserved && !Reserved->IsValid()) SceneActorReservations.Remove(Id);
	const TWeakObjectPtr<AActor>* Reservation = SceneActorReservations.Find(Id);
	const FGuid* ReservedId = SceneActorReservations.FindKey(ActorKey);
	if (!Id.IsValid() || SceneActorMap.Contains(Id) || SceneActorMap.FindKey(InActor) ||
		(Reservation && Reservation->Get() != InActor) || (ReservedId && *ReservedId != Id))
	{
		if (bReportConflict) UE_LOG(LogTemp, Error, TEXT("Scene actor identity is invalid or already registered: world=%s actor=%s id=%s"),
			*GetNameSafe(World), *GetNameSafe(InActor), *Id.ToString());
		return false;
	}
	SceneActorMap.Add(Id, InActor);
	SceneActorReservations.Remove(Id);
	bool bMarkerChanged = false;
	for (const auto& Pair : Markers) if (Pair.Value.ActorID == Id) { bMarkerChanged = true; break; }
	if (bMarkerChanged && bNotifyMarkers) NotifySceneMarkersChanged();
	return true;
}

bool USceneModule::RemoveSceneActor(AActor* InActor)
{
	if (!InActor || !GetWorld() || InActor->GetWorld() != GetWorld()) return false;
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	const bool bRemovedDeferred = DeferredSceneActorRegistrations.Remove(ActorKey) > 0;
	const FGuid* ReservedId = SceneActorReservations.FindKey(ActorKey);
	const bool bReleasedReservation = ReservedId != nullptr;
	if (ReservedId) { const FGuid Id = *ReservedId; SceneActorReservations.Remove(Id); }
	const FGuid* FoundId = SceneActorMap.FindKey(InActor);
	if (FoundId)
	{
		const FGuid ActorID = *FoundId;
		bool bMarkerChanged = false;
		for(auto& Iter : Markers)
		{
			if(Iter.Value.ActorID == ActorID)
			{
				Iter.Value.Location = InActor->GetActorLocation();
				bMarkerChanged = true;
			}
		}
		SceneActorMap.Remove(ActorID);
		if(bMarkerChanged) NotifySceneMarkersChanged();
		return true;
	}
	return bReleasedReservation || bRemovedDeferred;
}

bool USceneModule::QueueSceneActorRegistration(AActor* InActor, const FGuid& ExpectedId)
{
	if (!IsBusinessAdmissionOpen()) return false;
	check(IsInGameThread());
	UWorld* World = GetWorld();
	if (bSceneRuntimeClosing || !::IsValid(World) || World->bIsTearingDown || !::IsValid(InActor) ||
		InActor->GetWorld() != World || !ExpectedId.IsValid()) return false;
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	FDeferredSceneActorRegistration& Request = DeferredSceneActorRegistrations.FindOrAdd(ActorKey);
	Request.ExpectedId = ExpectedId;
	Request.Serial = ++SceneActorRegistrationSerial;
	if (!PendingSceneActorRegistrations.Contains(ActorKey))
	{
		if (const FGuid* Existing = SceneActorMap.FindKey(InActor); Existing && *Existing != ExpectedId)
		{
			const FGuid Previous = *Existing;
			if (!Request.PreviousId.IsValid()) Request.PreviousId = Previous;
			SceneActorMap.Remove(Previous);
		}
		if (const FGuid* Existing = SceneActorReservations.FindKey(ActorKey); Existing && *Existing != ExpectedId)
		{
			const FGuid Previous = *Existing;
			SceneActorReservations.Remove(Previous);
		}
	}
	return true;
}

void USceneModule::RetrySceneActorRegistrations()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (bRetryingSceneActorRegistrations || bSceneRuntimeClosing || DeferredSceneActorRegistrations.IsEmpty()) return;
	TGuardValue<bool> Guard(bRetryingSceneActorRegistrations, true);
	TArray<TWeakObjectPtr<AActor>> Actors;
	DeferredSceneActorRegistrations.GenerateKeyArray(Actors);
	for (const TWeakObjectPtr<AActor>& ActorKey : Actors)
	{
		if (bSceneRuntimeClosing) return;
		FDeferredSceneActorRegistration* Current = DeferredSceneActorRegistrations.Find(ActorKey);
		if (!Current) continue;
		FDeferredSceneActorRegistration Request = *Current;
		AActor* Actor = ActorKey.Get();
		if (!Actor || Actor->GetWorld() != GetWorld()) { DeferredSceneActorRegistrations.Remove(ActorKey); continue; }
		if (const FGuid* Existing = SceneActorMap.FindKey(Actor); Existing && *Existing != Request.ExpectedId)
		{
			const FGuid Previous = *Existing;
			if (!Current->PreviousId.IsValid()) Current->PreviousId = Previous;
			Request.PreviousId = Current->PreviousId;
			SceneActorMap.Remove(Previous);
		}
		if (const FGuid* Existing = SceneActorReservations.FindKey(ActorKey); Existing && *Existing != Request.ExpectedId)
		{
			const FGuid Previous = *Existing;
			SceneActorReservations.Remove(Previous);
		}
		const bool bRegistered = TryAddSceneActor(Actor, false, &Request.ExpectedId, false);
		if (bSceneRuntimeClosing) return;
		Current = DeferredSceneActorRegistrations.Find(ActorKey);
		if (!Current || Current->Serial != Request.Serial || !bRegistered) continue;
		DeferredSceneActorRegistrations.Remove(ActorKey);
		bool bMarkersChanged = false;
		for (const auto& Pair : Markers) if (Pair.Value.ActorID == Request.ExpectedId) { bMarkersChanged = true; break; }
		if (Request.PreviousId.IsValid() && Request.PreviousId != Request.ExpectedId)
			for (auto& Pair : Markers)
				if (Pair.Value.ActorID == Request.PreviousId) { Pair.Value.ActorID = Request.ExpectedId; bMarkersChanged = true; }
		if (bMarkersChanged) NotifySceneMarkersChanged();
	}
}

bool USceneModule::ReassignSceneActorIdentity(AActor* InActor, const FGuid& OldId, const FGuid& NewId, TFunctionRef<void()> ApplyIdentity)
{
	if (!IsBusinessAdmissionOpen()) return false;
	check(IsInGameThread());
	UWorld* World = GetWorld();
	if (bSceneRuntimeClosing || !::IsValid(World) || World->bIsTearingDown || !::IsValid(InActor) ||
		InActor->GetWorld() != World || !NewId.IsValid()) return false;
	const TWeakObjectPtr<AActor> ActorKey(InActor);
	if (PendingSceneActorRegistrations.Contains(ActorKey)) return false;
	const FGuid* Registered = SceneActorMap.FindKey(InActor);
	if (Registered && *Registered != OldId) return false;
	if (AActor* const* Existing = SceneActorMap.Find(NewId); Existing && *Existing != InActor) return false;
	if (TWeakObjectPtr<AActor>* Reserved = SceneActorReservations.Find(NewId); Reserved && !Reserved->IsValid()) SceneActorReservations.Remove(NewId);
	if (const TWeakObjectPtr<AActor>* Reserved = SceneActorReservations.Find(NewId); Reserved && Reserved->Get() != InActor) return false;
	const FGuid* OldReservation = SceneActorReservations.FindKey(ActorKey);
	if (OldReservation && *OldReservation != OldId) return false;
	const bool bRegistered = Registered != nullptr;
	const bool bHadReservation = OldReservation != nullptr;
	PendingSceneActorRegistrations.Add(ActorKey);
	ON_SCOPE_EXIT { PendingSceneActorRegistrations.Remove(ActorKey); };
	ApplyIdentity();
	if (bHadReservation && OldId != NewId) SceneActorReservations.Remove(OldId);
	if (!bRegistered) SceneActorReservations.Add(NewId, ActorKey);
	if (bRegistered && OldId != NewId)
	{
		SceneActorMap.Remove(OldId);
		SceneActorMap.Add(NewId, InActor);
		bool bMarkersChanged = false;
		for (auto& Pair : Markers)
		{
			if (Pair.Value.ActorID == OldId) { Pair.Value.ActorID = NewId; bMarkersChanged = true; }
		}
		if (bMarkersChanged) NotifySceneMarkersChanged();
	}
	return true;
}

void USceneModule::MarkSceneActorDestroyed(AActor* InActor)
{
	if (InActor && InActor->GetWorld() == GetWorld() && InActor->HasAnyFlags(RF_WasLoaded))
	{
		if (const FGuid* Id = SceneActorMap.FindKey(InActor)) DestroyedSceneActorIds.Add(*Id);
	}
}

bool USceneModule::HasTargetPointByName(const FName InName, bool bEnsured) const
{
	if(TargetPoints.Contains(InName)) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No TargetPoint, TargetPoint name: %s"), *InName.ToString()), EDC_Scene, EDV_Error);
	return false;
}

ATargetPoint* USceneModule::GetTargetPointByName(const FName InName, bool bEnsured) const
{
	if(HasTargetPointByName(InName, bEnsured))
	{
		return TargetPoints[InName];
	}
	return nullptr;
}

void USceneModule::AddTargetPointByName(const FName InName, ATargetPoint* InPoint)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!TargetPoints.Contains(InName))
	{
		TargetPoints.Add(InName, InPoint);
	}
	if(InPoint && InPoint->IsValidLowLevel())
	{
		AddScenePointByName(InName, InPoint->GetRootComponent());
	}
}

void USceneModule::RemoveTargetPointByName(const FName InName)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(TargetPoints.Contains(InName))
	{
		TargetPoints.Remove(InName);
	}
}

bool USceneModule::HasScenePointByName(const FName InName, bool bEnsured) const
{
	if(ScenePoints.Contains(InName)) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No ScenePoint, ScenePoint name: %s"), *InName.ToString()), EDC_Scene, EDV_Error);
	return false;
}

USceneComponent* USceneModule::GetScenePointByName(const FName InName, bool bEnsured) const
{
	if(HasScenePointByName(InName, bEnsured))
	{
		return ScenePoints[InName];
	}
	return nullptr;
}

void USceneModule::AddScenePointByName(const FName InName, USceneComponent* InSceneComp)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!ScenePoints.Contains(InName))
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

void USceneModule::RemoveScenePointByName(const FName InName)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(ScenePoints.Contains(InName))
	{
		ScenePoints.Remove(InName);
	}
}

bool USceneModule::HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
{
	if(!InClass) return false;

	if(APhysicsVolumeBase* DefaultPhysicsVolume = Cast<APhysicsVolumeBase>(InClass->GetDefaultObject()))
	{
		return HasPhysicsVolumeByName(DefaultPhysicsVolume->GetVolumeName(), bEnsured);
	}
	return false;
}

bool USceneModule::HasPhysicsVolumeByName(const FName InName, bool bEnsured) const
{
	if(PhysicsVolumes.Contains(InName)) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No PhysicsVolume, PhysicsVolume name: %s"), *InName.ToString()), EDC_Scene, EDV_Error);
	return false;
}

APhysicsVolumeBase* USceneModule::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
{
	if(!InClass) return nullptr;

	if(APhysicsVolumeBase* DefaultPhysicsVolume = Cast<APhysicsVolumeBase>(InClass->GetDefaultObject()))
	{
		return GetPhysicsVolumeByName(DefaultPhysicsVolume->GetVolumeName(), InClass, bEnsured);
	}
	return nullptr;
}

APhysicsVolumeBase* USceneModule::GetPhysicsVolumeByName(const FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
{
	if(HasPhysicsVolumeByName(InName, bEnsured))
	{
		return PhysicsVolumes[InName];
	}
	return nullptr;
}

void USceneModule::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!InPhysicsVolume) return;

	AddPhysicsVolumeByName(InPhysicsVolume->GetVolumeName(), InPhysicsVolume);
}

void USceneModule::AddPhysicsVolumeByName(const FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!InPhysicsVolume) return;

	if(!PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Add(InName, InPhysicsVolume);
	}
}

void USceneModule::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!InPhysicsVolume) return;

	RemovePhysicsVolumeByName(InPhysicsVolume->GetVolumeName());
}

void USceneModule::RemovePhysicsVolumeByName(const FName InName)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Remove(InName);
	}
}

void USceneModule::SpawnWorldText(const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(InOffsetRange != FVector::ZeroVector)
	{
		InMapping.Location = InMapping.Location + FMath::RandPointInBox(FBox(-InOffsetRange * 0.5f, InOffsetRange * 0.5f));
	}
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld());
	if (!Instance || !Widgets) return;
	for (ULocalPlayer* LocalPlayer : Instance->GetLocalPlayers())
	{
		APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
		if (!Controller) continue;
		FWorldTextWidgetParameter Param(InText, InTextColor, static_cast<int32>(InTextStyle));
		Param.OwningPlayer = Controller;
		Widgets->CreateWorldWidget<UWidgetWorldText>(InMapping, Param);
	}
}

FLinearColor USceneModule::GetOutlineColor() const
{
	return OutlineColor;
}

void USceneModule::SetOutlineColor(const FLinearColor& InColor)
{
	if (!IsBusinessAdmissionOpen()) return;
	OutlineColor = InColor;
	if(OutlineMatInst)
	{
		OutlineMatInst->SetVectorParameterValue("Color", OutlineColor);
	}
}
