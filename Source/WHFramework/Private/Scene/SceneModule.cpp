
#include "Scene/SceneModule.h"
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
#include "Widget/WidgetModule.h"
#include "Widget/WidgetModuleStatics.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

IMPLEMENTATION_MODULE(USceneModule)

USceneModule::USceneModule()
{
	ModuleName = FName("SceneModule");
	ModuleDisplayName = FText::FromString(TEXT("Scene Module"));
	SaveScope = ESaveScope::World;

	bModuleRequired = true;


	bSaveActorDatas = false;

	SeaLevel = 0.f;
	Altitude = 0.f;

	MiniMapCapture = nullptr;

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
	WorldMapMaxRange = 262144.f;
	
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> MiniMapTexFinder(TEXT("/Script/Engine.TextureRenderTarget2D'/WHFramework/Scene/Textures/Render/RT_MiniMap_Default.RT_MiniMap_Default'"));
	if(MiniMapTexFinder.Succeeded())
	{
		MiniMapTexture = MiniMapTexFinder.Object;
	}

	SceneAreas = TArray<FSceneArea>();
	Markers = TMap<FGuid, FSceneMarker>();
	TrackedMarkerID.Invalidate();
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

	AsyncLoadLevelQueue = TArray<FAsyncLoadLevelTask>();
}

USceneModule::~USceneModule()
{
	TERMINATION_MODULE(USceneModule)
}

#if WITH_EDITOR
void USceneModule::OnGenerate()
{
	if(!MiniMapCapture)
	{
		TArray<AActor*> ChildActors;
		GetModuleOwner()->GetAttachedActors(ChildActors);
		if(ChildActors.Num() > 0)
		{
			MiniMapCapture = Cast<AMiniMapCapture>(ChildActors[0]);
		}
	}
	if(!MiniMapCapture)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MiniMapCapture = GetWorld()->SpawnActor<AMiniMapCapture>(ActorSpawnParameters);
		if(MiniMapCapture)
		{
			MiniMapCapture->SetActorLabel(TEXT("MiniMapCapture"));
			MiniMapCapture->AttachToActor(GetModuleOwner(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	if(MiniMapCapture)
	{
		MiniMapCapture->GetCapture()->TextureTarget = MiniMapTexture;
		MiniMapCapture->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	Modify();
}

void USceneModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(USceneModule)

	if(MiniMapCapture)
	{
		MiniMapCapture->Destroy();
	}
}
#endif

void USceneModule::OnInitialize()
{
	Super::OnInitialize();

	IDebuggerInterface::Register();
	if(GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		CameraStreamingBridge = GetWorld()->SpawnActor<ACameraStreamingBridge>();
	}

	UEventModuleStatics::SubscribeEvent<FEventAsyncLoadLevels>(this, &ThisClass::OnAsyncLoadLevels);
	UEventModuleStatics::SubscribeEvent<FEventAsyncUnloadLevels>(this, &ThisClass::OnAsyncUnloadLevels);
	UEventModuleStatics::SubscribeEvent<FEventSetActorVisible>(this, &ThisClass::OnSetActorVisible);
	UEventModuleStatics::SubscribeEvent<FEventPlayLevelSequence>(this, &ThisClass::OnPlayLevelSequence);
	UEventModuleStatics::SubscribeEvent<FEventStopLevelSequence>(this, &ThisClass::OnStopLevelSequence);
	UEventModuleStatics::SubscribeEvent<FEventSetDataLayerRuntimeState>(this, &ThisClass::OnSetDataLayerRuntimeState);
	UEventModuleStatics::SubscribeEvent<FEventSetDataLayerOwnerPlayer>(this, &ThisClass::OnSetDataLayerOwnerPlayer);
	UEventModuleStatics::SubscribeEvent<FEventSetLevelOwnerPlayer>(this, &ThisClass::OnSetLevelOwnerPlayer);

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
		if(MiniMapCapture)
		{
			MiniMapCapture->GetCapture()->SetActive(MiniMapSource == EWorldMiniMapSource::SceneCapture && MiniMapMode != EWorldMiniMapMode::None, true);
		}
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
	Super::OnRefresh(DeltaSeconds, bInEditor);
	
	if(bInEditor) return;

	FSceneArea PendingSceneArea;
	while(PendingSceneAreas.Dequeue(PendingSceneArea))
	{
		AddSceneArea(PendingSceneArea);
	}

	Altitude = UCameraModuleStatics::GetViewLocation().Z - SeaLevel;
	RefreshWorldMarkerWidgets();

	if(MiniMapCapture && MiniMapSource == EWorldMiniMapSource::SceneCapture)
	{
		switch(MiniMapMode)
		{
			case EWorldMiniMapMode::FixedPoint:
			{
				MiniMapCapture->SetActorLocationAndRotation(MiniMapPoint.GetLocation(), bMiniMapRotatable ? FRotator(0.f, MiniMapPoint.Rotator().Yaw, 0.f) : FRotator::ZeroRotator);
				break;
			}
			case EWorldMiniMapMode::ViewPoint:
			{
				if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
				{
					if(const AActor* ViewTarget = PlayerController->GetViewTarget())
					{
						MiniMapCapture->SetActorLocationAndRotation(ViewTarget->GetActorLocation(), bMiniMapRotatable ? FRotator(0.f, ViewTarget->GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator);
					}
				}
				break;
			}
			case EWorldMiniMapMode::CameraPoint:
			{
				const FTransform ViewTransform = UCameraModuleStatics::GetViewTransform();
				MiniMapCapture->SetActorLocationAndRotation(ViewTransform.GetLocation(), bMiniMapRotatable ? FRotator(0.f, ViewTransform.Rotator().Yaw, 0.f) : FRotator::ZeroRotator);
				break;
			}
			default: break;
		}
		MiniMapCapture->GetCapture()->OrthoWidth = MiniMapRange;
	}
	
	if(WorldTimer)
	{
		WorldTimer->OnRefresh(DeltaSeconds);
	}
	
	if(WorldWeather)
	{
		WorldWeather->OnRefresh(DeltaSeconds);
	}
	
	if(AsyncLoadLevelQueue.Num() > 0)
	{
		FAsyncLoadLevelTask& Task = AsyncLoadLevelQueue[0];
		if(!Task.bLoading)
		{
			switch(Task.State)
			{
				case EFAsyncLoadLevelState::Loading:
				{
					AsyncLoadLevelInternal(Task);
					break;
				}
				case EFAsyncLoadLevelState::Unloading:
				{
					AsyncUnloadLevelInternal(Task);
					break;
				}
				default: break;
			}
		}
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
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		IDebuggerInterface::UnRegister();
		if(CameraStreamingBridge)
		{
			CameraStreamingBridge->Destroy();
			CameraStreamingBridge = nullptr;
		}

		FSceneArea Area;
		while(PendingSceneAreas.Dequeue(Area)) { }
		ClearWorldMarkerWidgets();
		ClearMarkers(true);
	}
}

void USceneModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData.GetRef<FSceneModuleSaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		if(SaveData.IsSaved())
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
			TrackedMarkerID = Markers.Contains(SaveData.TrackedMarkerID) ? SaveData.TrackedMarkerID : FGuid();
			NotifySceneMarkersChanged();
		}
		
		if(WorldTimer && WorldTimer->IsAutoSave())
		{
			// WorldTimer->LoadSaveData(FParameter(SaveData.TimerData));
		}
		
		if(WorldWeather && WorldWeather->IsAutoSave())
		{
			// WorldWeather->LoadSaveData(FParameter(SaveData.WeatherData));
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
			for(const FSceneActorSaveRecord& Record : SaveData.ActorSaveRecords)
			{
				AActor* Actor = GetSceneActor(Record.ActorId.ToString(), nullptr, false);
				if(PHASEC(InPhase, EPhase::Primary))
				{
					if(Record.bDestroyed)
					{
						if(Actor)
						{
							Actor->Destroy();
						}
						DestroyedSceneActorIds.Add(Record.ActorId);
						continue;
					}
					if(!Actor && Record.bRuntimeSpawned && !Record.ActorClass.IsNull())
					{
						if(UClass* ActorClass = Record.ActorClass.TryLoadClass<AActor>())
						{
							Actor = GetWorld()->SpawnActorDeferred<AActor>(ActorClass, Record.Transform);
							if(Actor && Actor->Implements<USceneActorInterface>())
							{
								ISceneActorInterface::Execute_SetActorID(Actor, Record.ActorId.ToString());
								UGameplayStatics::FinishSpawningActor(Actor, Record.Transform);
								AddSceneActor(Actor);
							}
						}
					}
				}
				if(Actor)
				{
					Actor->SetActorTransform(Record.Transform);
					if(ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Actor))
					{
						Agent->LoadSaveData(Record.Data, InPhase);
					}
				}
			}
		}
	}
}

FParameter USceneModule::ToData()
{
	FSceneModuleSaveData SaveData;

	SaveData.MiniMapRange = MiniMapRange;
	SaveData.WorldMapCenter = WorldMapCenter;
	SaveData.WorldMapRange = WorldMapRange;
	SaveData.TrackedMarkerID = TrackedMarkerID;
	SaveData.SceneAreas = SceneAreas;
	for(const auto& Iter : Markers)
	{
		if(Iter.Value.bPersistent) SaveData.Markers.Add(Iter.Value);
	}
	
	if(WorldTimer && WorldTimer->IsAutoSave())
	{
		SaveData.TimerData = WorldTimer->GetSaveData(true).GetRef<FWorldTimerSaveData>();
	}
	
	if(WorldWeather && WorldWeather->IsAutoSave())
	{
		SaveData.WeatherData = WorldWeather->GetSaveData(true).GetRef<FWorldWeatherSaveData>();
	}

	if(bSaveActorDatas)
	{
		for(const TPair<FGuid, AActor*>& Iter : SceneActorMap)
		{
			AActor* Actor = Iter.Value;
			if(!::IsValid(Actor))
			{
				continue;
			}
			if(ISaveDataAgentInterface* Agent = Cast<ISaveDataAgentInterface>(Actor))
			{
				FSceneActorSaveRecord Record;
				Record.ActorId = Iter.Key;
				Record.ActorClass = FSoftClassPath(Actor->GetClass());
				Record.Transform = Actor->GetActorTransform();
				Record.bRuntimeSpawned = !Actor->HasAnyFlags(RF_WasLoaded);
				Record.Data = Agent->GetSaveData(true);
				if(Actor->Implements<UPrimaryEntityInterface>())
				{
					Record.AssetId = IPrimaryEntityInterface::Execute_GetAssetID(Actor);
				}
				SaveData.ActorSaveRecords.Add(MoveTemp(Record));
			}
		}
		for(const FGuid& ActorId : DestroyedSceneActorIds)
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

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(USceneModule, MiniMapTexture))
		{
			MiniMapCapture->GetCapture()->TextureTarget = MiniMapTexture;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

float USceneModule::GetAltitude(bool bUnsigned, bool bRefresh) const
{
	const float ReturnValue = bRefresh ? UCameraModuleStatics::GetViewLocation().Z - SeaLevel : Altitude;
	return bUnsigned ? FMath::Max(ReturnValue, 0.f) : ReturnValue;
}

void USceneModule::SetMiniMapMode(EWorldMiniMapMode InMiniMapMode)
{
	MiniMapMode = InMiniMapMode;
	if(MiniMapCapture)
	{
		MiniMapCapture->GetCapture()->SetActive(MiniMapSource == EWorldMiniMapSource::SceneCapture && MiniMapMode != EWorldMiniMapMode::None, true);
	}
}

void USceneModule::SetMiniMapTexture(UTextureRenderTarget2D* InMiniMapTexture)
{
	MiniMapTexture = InMiniMapTexture;

	if(MiniMapCapture) MiniMapCapture->GetCapture()->TextureTarget = MiniMapTexture;
}

FSceneMapView USceneModule::GetMapView(ESceneMarkerChannel InChannel) const
{
	FSceneMapView View;
	if(InChannel == ESceneMarkerChannel::Map)
	{
		View.Center = WorldMapCenter;
		View.Range = WorldMapRange;
		return View;
	}
	if(InChannel != ESceneMarkerChannel::MiniMap) return View;

	FVector Location = MiniMapPoint.GetLocation();
	float Yaw = MiniMapPoint.Rotator().Yaw;
	switch(MiniMapMode)
	{
		case EWorldMiniMapMode::ViewPoint:
		{
			if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
			{
				if(const AActor* ViewTarget = PlayerController->GetViewTarget())
				{
					Location = ViewTarget->GetActorLocation();
					Yaw = ViewTarget->GetActorRotation().Yaw;
				}
			}
			break;
		}
		case EWorldMiniMapMode::CameraPoint:
		{
			const FTransform ViewTransform = UCameraModuleStatics::GetViewTransform();
			Location = ViewTransform.GetLocation();
			Yaw = ViewTransform.Rotator().Yaw;
			break;
		}
		default: break;
	}
	View.Center = FVector2D(Location);
	View.Range = MiniMapRange;
	View.Yaw = bMiniMapRotatable ? Yaw : 0.f;
	return View;
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
		PendingSceneAreas.Enqueue(InArea);
		return;
	}

	FSceneArea Area = InArea;
	if(Area.AreaName.IsNone())
	{
		Area.AreaName = *FString::Printf(TEXT("Area_%d"), SceneAreas.Num());
	}
	if(!HasSceneArea(Area.AreaName))
	{
		SceneAreas.Add(Area);
		OnSceneAreaAdded.Broadcast(Area);
		OnSceneAreasChanged.Broadcast();
	}
}

void USceneModule::RemoveSceneArea(const FName InName)
{
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
	const bool bHadSceneAreas = !SceneAreas.IsEmpty();
	SceneAreas.Empty();

	FSceneArea Area;
	while(PendingSceneAreas.Dequeue(Area)) { }
	if(bHadSceneAreas) OnSceneAreasChanged.Broadcast();
}

FGuid USceneModule::AddMarker(const FSceneMarker& InMarker)
{
	FSceneMarker Marker = InMarker;
	if(!Marker.MarkerID.IsValid()) Marker.MarkerID = FGuid::NewGuid();
	if(Markers.Contains(Marker.MarkerID)) return FGuid();
	Markers.Add(Marker.MarkerID, Marker);
	NotifySceneMarkersChanged();
	return Marker.MarkerID;
}

bool USceneModule::UpdateMarker(const FSceneMarker& InMarker)
{
	FSceneMarker* Existing = InMarker.MarkerID.IsValid() ? Markers.Find(InMarker.MarkerID) : nullptr;
	if(!Existing) return false;
	if(Existing->MarkerTag == InMarker.MarkerTag && Existing->DisplayName.EqualTo(InMarker.DisplayName) && Existing->Icon == InMarker.Icon &&
		Existing->Color.Equals(InMarker.Color) && Existing->Location.Equals(InMarker.Location) && Existing->Offset.Equals(InMarker.Offset) &&
		Existing->ActorID == InMarker.ActorID && Existing->AreaName == InMarker.AreaName && Existing->Channels == InMarker.Channels &&
		Existing->Priority == InMarker.Priority && FMath::IsNearlyEqual(Existing->MinDistance, InMarker.MinDistance) &&
		FMath::IsNearlyEqual(Existing->MaxDistance, InMarker.MaxDistance) && Existing->bPersistent == InMarker.bPersistent) return true;
	*Existing = InMarker;
	NotifySceneMarkersChanged();
	return true;
}

bool USceneModule::RemoveMarker(FGuid InMarkerID)
{
	if(!Markers.Remove(InMarkerID)) return false;
	if(TrackedMarkerID == InMarkerID) TrackedMarkerID.Invalidate();
	NotifySceneMarkersChanged();
	return true;
}

void USceneModule::ClearMarkers(bool bIncludePersistent)
{
	bool bChanged = false;
	for(auto Iter = Markers.CreateIterator(); Iter; ++Iter)
	{
		if(bIncludePersistent || !Iter.Value().bPersistent)
		{
			if(TrackedMarkerID == Iter.Key()) TrackedMarkerID.Invalidate();
			Iter.RemoveCurrent();
			bChanged = true;
		}
	}
	if(bChanged) NotifySceneMarkersChanged();
}

FSceneMarker USceneModule::GetMarker(FGuid InMarkerID) const
{
	if(const FSceneMarker* Marker = Markers.Find(InMarkerID)) return *Marker;
	return FSceneMarker();
}

FSceneMarkerView USceneModule::ResolveMarker(const FSceneMarker& InMarker, const FVector& InViewLocation, float InViewYaw) const
{
	FSceneMarkerView View;
	View.Marker = InMarker;
	View.Location = InMarker.Location;
	if(InMarker.ActorID.IsValid())
	{
		if(AActor* const* Actor = SceneActorMap.Find(InMarker.ActorID); Actor != nullptr && ::IsValid(*Actor))
		{
			View.Location = (*Actor)->GetActorLocation();
			View.bTargetLoaded = true;
		}
	}
	else if(!InMarker.AreaName.IsNone())
	{
		const FSceneArea Area = GetSceneArea(InMarker.AreaName);
		if(!Area.AreaName.IsNone())
		{
			View.Location = Area.EntranceLocation.IsNearlyZero() ? FVector(Area.AreaCenter, InMarker.Location.Z) : Area.EntranceLocation;
			View.bTargetLoaded = true;
		}
	}
	View.Location += InMarker.Offset;
	const FVector2D Delta = FVector2D(View.Location - InViewLocation);
	View.Distance = Delta.Size();
	const float DirectionYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	View.Bearing = FMath::FindDeltaAngleDegrees(InViewYaw, DirectionYaw);
	View.bTracked = TrackedMarkerID == InMarker.MarkerID;
	return View;
}

FSceneMarkerView USceneModule::GetMarkerView(FGuid InMarkerID, FVector InViewLocation, float InViewYaw) const
{
	if(const FSceneMarker* Marker = Markers.Find(InMarkerID)) return ResolveMarker(*Marker, InViewLocation, InViewYaw);
	return FSceneMarkerView();
}

TArray<FSceneMarkerView> USceneModule::GetMarkerViews(ESceneMarkerChannel InChannel, FVector InViewLocation, float InViewYaw, bool bFilterByDistance) const
{
	TArray<FSceneMarkerView> Views;
	for(const auto& Iter : Markers)
	{
		if(!Iter.Value.HasChannel(InChannel)) continue;
		FSceneMarkerView View = ResolveMarker(Iter.Value, InViewLocation, InViewYaw);
		if(bFilterByDistance && (View.Distance < Iter.Value.MinDistance || (Iter.Value.MaxDistance > 0.f && View.Distance > Iter.Value.MaxDistance))) continue;
		Views.Add(MoveTemp(View));
	}
	Views.StableSort([](const FSceneMarkerView& A, const FSceneMarkerView& B)
	{
		return A.Marker.Priority == B.Marker.Priority ? A.Distance < B.Distance : A.Marker.Priority > B.Marker.Priority;
	});
	return Views;
}

void USceneModule::RefreshWorldMarkerWidgets()
{
	if(!UWidgetModule::IsValid()) return;

	if(bWorldMarkerWidgetsDirty)
	{
		TSet<FGuid> DesiredIDs;
		for(const auto& Pair : Markers)
		{
			if(!Pair.Value.HasChannel(ESceneMarkerChannel::World)) continue;
			DesiredIDs.Add(Pair.Key);
			UWidgetSceneWorldMarker*& Widget = WorldMarkerWidgets.FindOrAdd(Pair.Key);
			if(!::IsValid(Widget))
			{
				Widget = UWidgetModule::Get().CreateWorldWidget<UWidgetSceneWorldMarker>(
					FWorldWidgetMapping(Pair.Value.Location + Pair.Value.Offset),
					FWidgetSpawnParameter(this),
					UWidgetSceneWorldMarker::StaticClass());
			}
			if(Widget)
			{
				Widget->SetMarkerView(ResolveMarker(Pair.Value, FVector::ZeroVector, 0.f));
			}
		}

		for(auto Iter = WorldMarkerWidgets.CreateIterator(); Iter; ++Iter)
		{
			if(DesiredIDs.Contains(Iter.Key())) continue;
			if(::IsValid(Iter.Value()))
			{
				UWidgetModule::Get().DestroyWorldWidget(Iter.Value(), EObjectDespawnMode::Recovery);
			}
			Iter.RemoveCurrent();
		}
		bWorldMarkerWidgetsDirty = false;
	}

	const FVector ViewLocation = UCameraModuleStatics::GetViewLocation();
	float ViewYaw = 0.f;
	if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
	{
		ViewYaw = PlayerController->GetControlRotation().Yaw;
	}
	for(const auto& Pair : WorldMarkerWidgets)
	{
		const FSceneMarker* Marker = Markers.Find(Pair.Key);
		if(Marker && ::IsValid(Pair.Value))
		{
			Pair.Value->UpdateMarkerState(ResolveMarker(*Marker, ViewLocation, ViewYaw));
		}
	}
}

void USceneModule::NotifySceneMarkersChanged()
{
	bWorldMarkerWidgetsDirty = true;
	OnSceneMarkersChanged.Broadcast();
}

void USceneModule::ClearWorldMarkerWidgets()
{
	if(UWidgetModule::IsValid())
	{
		for(const auto& Pair : WorldMarkerWidgets)
		{
			if(::IsValid(Pair.Value)) UWidgetModule::Get().DestroyWorldWidget(Pair.Value, EObjectDespawnMode::Recovery);
		}
	}
	WorldMarkerWidgets.Reset();
}

bool USceneModule::SetTrackedMarker(FGuid InMarkerID)
{
	if(InMarkerID.IsValid() && !Markers.Contains(InMarkerID)) return false;
	if(TrackedMarkerID == InMarkerID) return true;
	TrackedMarkerID = InMarkerID;
	NotifySceneMarkersChanged();
	return true;
}

UWorldTimer* USceneModule::GetWorldTimer(TSubclassOf<UWorldTimer> InClass) const
{
	return GetDeterminesOutputObject(WorldTimer, InClass);
}

UWorldWeather* USceneModule::GetWorldWeather(TSubclassOf<UWorldWeather> InClass) const
{
	return GetDeterminesOutputObject(WorldWeather, InClass);
}

void USceneModule::OnAsyncLoadLevels(UObject* InSender, const FEventAsyncLoadLevels& InEvent)
{
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
	if(AActor* Actor = InEvent.ActorPath.LoadSynchronous())
	{
		Actor->GetRootComponent()->SetVisibility(InEvent.bVisible, true);
	}
}

void USceneModule::OnPlayLevelSequence(UObject* InSender, const FEventPlayLevelSequence& InEvent)
{
	if(ALevelSequenceActor* Actor = InEvent.LevelSequence.LoadSynchronous())
	{
		FTimerHandle TimerHandle;
		auto PlaySequence = [Actor, InEvent]()
		{
			if(!InEvent.bReverse)
			{
				Actor->GetSequencePlayer()->Play();
			}
			else
			{
				Actor->GetSequencePlayer()->PlayReverse();
			}
		};
		if(InEvent.Delay > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [PlaySequence]()
			{
				PlaySequence();
			}, InEvent.Delay, false);
		}
		else
		{
			PlaySequence();
		}
	}
}

void USceneModule::OnStopLevelSequence(UObject* InSender, const FEventStopLevelSequence& InEvent)
{
	if(ALevelSequenceActor* Actor = InEvent.LevelSequence.LoadSynchronous())
	{
		FTimerHandle TimerHandle;
		auto StopSequence = [Actor, InEvent]()
		{
			if(!InEvent.bKeepState)
			{
				Actor->GetSequencePlayer()->Stop();
			}
			else
			{
				Actor->GetSequencePlayer()->StopAtCurrentTime();
			}
		};
		if(InEvent.Delay > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [StopSequence]()
			{
				StopSequence();
			}, InEvent.Delay, false);
		}
		else
		{
			StopSequence();
		}
	}
}

void USceneModule::OnSetDataLayerRuntimeState(UObject* InSender, const FEventSetDataLayerRuntimeState& InEvent)
{
	if(UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(this))
	{
		DataLayerManager->SetDataLayerRuntimeState(InEvent.DataLayer, InEvent.State, InEvent.bRecursive);
		ApplyDataLayerOwnerPlayer(InEvent.DataLayer);
	}
}

void USceneModule::OnSetDataLayerOwnerPlayer(UObject* InSender, const FEventSetDataLayerOwnerPlayer& InEvent)
{
	if(InEvent.PlayerIndex != -1)
	{
		DataLayerPlayerMappings.Emplace(InEvent.DataLayer, InEvent.PlayerIndex);
		ApplyDataLayerOwnerPlayer(InEvent.DataLayer);
	}
	else if(DataLayerPlayerMappings.Contains(InEvent.DataLayer))
	{
		DataLayerPlayerMappings.Remove(InEvent.DataLayer);
		ApplyOwnerPlayerToActors(UCommonModuleStatics::GetAllActorsOfDataLayer(InEvent.DataLayer), INDEX_NONE);
	}
}

void USceneModule::OnSetLevelOwnerPlayer(UObject* InSender, const FEventSetLevelOwnerPlayer& InEvent)
{
	const FName LevelPath = InEvent.LevelObjectPtr ? FName(*FPackageName::ObjectPathToPackageName(InEvent.LevelObjectPtr.ToString())) : InEvent.LevelPath;
	if(InEvent.PlayerIndex != -1)
	{
		LevelPlayerMappings.Emplace(LevelPath, InEvent.PlayerIndex);
		ApplyLevelOwnerPlayer(LevelPath);
	}
	else if(LevelPlayerMappings.Contains(LevelPath))
	{
		LevelPlayerMappings.Remove(LevelPath);
		ApplyOwnerPlayerToActors(UCommonModuleStatics::GetAllActorsOfLevel(LevelPath), INDEX_NONE);
	}
}

void USceneModule::ApplyOwnerPlayerToActors(const TArray<AActor*>& InActors, int32 InPlayerIndex) const
{
	AActor* OwnerActor = nullptr;
	if(InPlayerIndex != INDEX_NONE)
	{
		const AWHPlayerController* PlayerController = UCommonModuleStatics::GetPlayerController(InPlayerIndex);
		OwnerActor = PlayerController ? PlayerController->GetViewTarget() : nullptr;
		if(!OwnerActor) return;
	}

	for(AActor* Actor : InActors)
	{
		if(!::IsValid(Actor)) continue;
		Actor->SetOwner(OwnerActor);
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents<UPrimitiveComponent>(Components);
		for(UPrimitiveComponent* Component : Components)
		{
			if(Component) Component->SetOnlyOwnerSee(OwnerActor != nullptr);
		}
	}
}

void USceneModule::ApplyDataLayerOwnerPlayer(UDataLayerAsset* InDataLayer) const
{
	const int32* PlayerIndex = DataLayerPlayerMappings.Find(InDataLayer);
	if(!PlayerIndex) return;
	ApplyOwnerPlayerToActors(UCommonModuleStatics::GetAllActorsOfDataLayer(InDataLayer), *PlayerIndex);
}

void USceneModule::ApplyLevelOwnerPlayer(FName InLevelPath) const
{
	const int32* PlayerIndex = LevelPlayerMappings.Find(InLevelPath);
	if(!PlayerIndex) return;
	ApplyOwnerPlayerToActors(UCommonModuleStatics::GetAllActorsOfLevel(InLevelPath), *PlayerIndex);
}

void USceneModule::ApplyAllOwnerPlayerMappings() const
{
	for(const auto& Pair : DataLayerPlayerMappings)
	{
		ApplyDataLayerOwnerPlayer(Pair.Key);
	}
	for(const auto& Pair : LevelPlayerMappings)
	{
		ApplyLevelOwnerPlayer(Pair.Key);
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
	if(!TraceMappings.Contains(InName))
	{
		TraceMappings.Add(InName, InTraceChannel);
	}
}

void USceneModule::RemoveTraceMapping(const FName InName)
{
	if(TraceMappings.Contains(InName))
	{
		TraceMappings.Remove(InName);
	}
}

bool USceneModule::HasSceneActor(const FString& InID, bool bEnsured) const
{
	if(SceneActorMap.Contains(FGuid(InID))) return true;
	ensureEditorMsgf(!bEnsured, FString::Printf(TEXT("No scene actor, actor id: %s"), *InID), EDC_Scene, EDV_Error);
	return false;
}

AActor* USceneModule::GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(HasSceneActor(InID, bEnsured))
	{
		return GetDeterminesOutputObject(SceneActorMap[FGuid(InID)], InClass);
	}
	return nullptr;
}

bool USceneModule::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(!SceneActorMap.Contains(ISceneActorInterface::Execute_GetActorID(InActor)))
	{
		SceneActorMap.Add(ISceneActorInterface::Execute_GetActorID(InActor), InActor);
		for(const auto& Iter : Markers)
		{
			if(Iter.Value.ActorID == ISceneActorInterface::Execute_GetActorID(InActor))
			{
				NotifySceneMarkersChanged();
				break;
			}
		}
		return true;
	}
	return false;
}

bool USceneModule::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(SceneActorMap.Contains(ISceneActorInterface::Execute_GetActorID(InActor)))
	{
		const FGuid ActorID = ISceneActorInterface::Execute_GetActorID(InActor);
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
	return false;
}

void USceneModule::MarkSceneActorDestroyed(AActor* InActor)
{
	if(InActor && InActor->Implements<USceneActorInterface>() && InActor->HasAnyFlags(RF_WasLoaded))
	{
		DestroyedSceneActorIds.Add(ISceneActorInterface::Execute_GetActorID(InActor));
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
	if(!ScenePoints.Contains(InName))
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

void USceneModule::RemoveScenePointByName(const FName InName)
{
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
	if(!InPhysicsVolume) return;
	
	AddPhysicsVolumeByName(InPhysicsVolume->GetVolumeName(), InPhysicsVolume);
}

void USceneModule::AddPhysicsVolumeByName(const FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(!InPhysicsVolume) return;
	
	if(!PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Add(InName, InPhysicsVolume);
	}
}

void USceneModule::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(!InPhysicsVolume) return;

	RemovePhysicsVolumeByName(InPhysicsVolume->GetVolumeName());
}

void USceneModule::RemovePhysicsVolumeByName(const FName InName)
{
	if(PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Remove(InName);
	}
}

void USceneModule::SpawnWorldText(const FString& InText, const FLinearColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange)
{
	if(InOffsetRange != FVector::ZeroVector)
	{
		InMapping.Location = InMapping.Location + FMath::RandPointInBox(FBox(-InOffsetRange * 0.5f, InOffsetRange * 0.5f));
	}
	UWidgetModuleStatics::CreateWorldWidget<UWidgetWorldText>(
		InMapping,
		FWorldTextWidgetParameter(InText, InTextColor, static_cast<int32>(InTextStyle)));
}

FLinearColor USceneModule::GetOutlineColor() const
{
	return OutlineColor;
}

void USceneModule::SetOutlineColor(const FLinearColor& InColor)
{
	OutlineColor = InColor;
	if(OutlineMatInst)
	{
		OutlineMatInst->SetVectorParameterValue("Color", OutlineColor);
	}
}

void USceneModule::AsyncLoadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	bool bExistTask = false;
	ITER_ARRAY_WITHINDEX(AsyncLoadLevelQueue, Index, Item,
		if(Item.LevelPath == InLevelPath)
		{
			if(Item.State == EFAsyncLoadLevelState::Loading)
			{
				bExistTask = true;
			}
			else if(!AsyncLoadLevelQueue[Index].bLoading)
			{
				AsyncLoadLevelQueue.RemoveAt(Index);
			}
			break;
		}
	)
	if(!bExistTask)
	{
		FAsyncLoadLevelTask Task;
		Task.State = EFAsyncLoadLevelState::Loading;
		Task.LevelPath = InLevelPath;
		Task.OnLoadFinished = InOnLoadFinished;
		Task.FinishDelayTime = InFinishDelayTime;
		Task.bCreateLoadingWidget = bCreateLoadingWidget;

		AsyncLoadLevelQueue.Add(Task);
	}
}

void USceneModule::AsyncLoadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnLoadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	const FName LevelPath = FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString()));
	AsyncLoadLevel(LevelPath, InOnLoadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

void USceneModule::AsyncUnloadLevel(const FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	bool bExistTask = false;
	ITER_ARRAY_WITHINDEX(AsyncLoadLevelQueue, Index, Item,
		if(Item.LevelPath == InLevelPath)
		{
			if(Item.State == EFAsyncLoadLevelState::Unloading)
			{
				bExistTask = true;
			}
			else if(!AsyncLoadLevelQueue[Index].bLoading)
			{
				AsyncLoadLevelQueue.RemoveAt(Index);
			}
			break;
		}
	)
	if(!bExistTask)
	{
		FAsyncLoadLevelTask Task;
		Task.State = EFAsyncLoadLevelState::Unloading;
		Task.LevelPath = InLevelPath;
		Task.OnLoadFinished = InOnUnloadFinished;
		Task.FinishDelayTime = InFinishDelayTime;
		Task.bCreateLoadingWidget = bCreateLoadingWidget;

		AsyncLoadLevelQueue.Add(Task);
	}
}

void USceneModule::AsyncUnloadLevelByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr, const FOnAsyncLoadLevelFinished& InOnUnloadFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	const FName LevelPath = FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString()));
	AsyncUnloadLevel(LevelPath, InOnUnloadFinished, InFinishDelayTime, bCreateLoadingWidget);
}

float USceneModule::GetAsyncLoadLevelProgress(const FName InLevelPath) const
{
	return 1.f;
}

float USceneModule::GetAsyncLoadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr) const
{
	const FName LevelPath = FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString()));
	return GetAsyncLoadLevelProgress(LevelPath);
}

float USceneModule::GetAsyncUnloadLevelProgress(const FName InLevelPath) const
{
	return 1.f;
}

float USceneModule::GetAsyncUnloadLevelProgressByObjectPtr(const TSoftObjectPtr<UWorld> InLevelObjectPtr) const
{
	const FName LevelPath = FName(*FPackageName::ObjectPathToPackageName(InLevelObjectPtr.ToString()));
	return GetAsyncUnloadLevelProgress(LevelPath);
}

void USceneModule::AsyncLoadLevelInternal(FAsyncLoadLevelTask& InTask)
{
	WHLog(TEXT("Start load level: %s") + InTask.LevelPath.ToString(), EDC_Scene);

	const FString LoadPackagePath = FPaths::GetBaseFilename(InTask.LevelPath.ToString(), false);

	InTask.bLoading = true;

	if(InTask.bCreateLoadingWidget)
	{
		UWidgetModuleStatics::OpenUserWidget<UWidgetLoadingLevelPanel>(
			FLoadingLevelWidgetOpenParameter(InTask.LevelPath, false));
	}
	
	LoadPackageAsync(LoadPackagePath, FLoadPackageAsyncDelegate::CreateLambda([this, InTask](const FName PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result){
		if(Result == EAsyncLoadingResult::Failed)
		{
			WHLog(TEXT("Load level failed!"));
		}
		else if(Result == EAsyncLoadingResult::Succeeded)
		{
			FLatentActionInfo LatentActionInfo;
			LatentActionInfo.UUID = FMath::Rand();
			LatentActionInfo.Linkage = 0;
			LatentActionInfo.CallbackTarget = this;
			LatentActionInfo.ExecutionFunction = GET_FUNCTION_NAME_THISCLASS(OnHandleAsyncLoadLevelFinish);
			UGameplayStatics::LoadStreamLevel(this, InTask.LevelPath, true, false, LatentActionInfo);
		}
	}), 0, PKG_ContainsMap);
}

void USceneModule::AsyncUnloadLevelInternal(FAsyncLoadLevelTask& InTask)
{
	WHLog(TEXT("Start unload level: ") + InTask.LevelPath.ToString());

	InTask.bLoading = true;

	if(InTask.bCreateLoadingWidget)
	{
		UWidgetModuleStatics::OpenUserWidget<UWidgetLoadingLevelPanel>(
			FLoadingLevelWidgetOpenParameter(InTask.LevelPath, true));
	}

	FLatentActionInfo LatentActionInfo;
	LatentActionInfo.UUID = FMath::Rand();
	LatentActionInfo.Linkage = 0;
	LatentActionInfo.CallbackTarget = this;
	LatentActionInfo.ExecutionFunction = GET_FUNCTION_NAME_THISCLASS(OnHandleAsyncUnloadLevelFinish);
	UGameplayStatics::UnloadStreamLevel(this, InTask.LevelPath, LatentActionInfo, false);
}

void USceneModule::OnAsyncLoadLevelFinished(FAsyncLoadLevelTask InTask)
{
	WHLog(TEXT("Load level Succeeded!"));
	ApplyAllOwnerPlayerMappings();

	if(InTask.bCreateLoadingWidget)
	{
		UWidgetModuleStatics::CloseUserWidget<UWidgetLoadingLevelPanel>();
	}

	if(InTask.OnLoadFinished.IsBound())
	{
		InTask.OnLoadFinished.Execute(InTask.LevelPath);
	}
	UEventModuleStatics::BroadcastEvent<FEventAsyncLoadLevelFinished>(this, { InTask.LevelPath }, EEventNetType::Multicast);
}

void USceneModule::OnAsyncUnloadLevelFinished(FAsyncLoadLevelTask InTask)
{
	WHLog(TEXT("Unload level Succeeded!"));

	if(InTask.bCreateLoadingWidget)
	{
		UWidgetModuleStatics::CloseUserWidget<UWidgetLoadingLevelPanel>();
	}
	
	if(InTask.OnLoadFinished.IsBound())
	{
		InTask.OnLoadFinished.Execute(InTask.LevelPath);
	}
	UEventModuleStatics::BroadcastEvent<FEventAsyncUnloadLevelFinished>(this, { InTask.LevelPath }, EEventNetType::Multicast);
}

void USceneModule::OnHandleAsyncLoadLevelFinish()
{
	if(AsyncLoadLevelQueue.IsValidIndex(0))
	{
		FAsyncLoadLevelTask Task = AsyncLoadLevelQueue[0];
		AsyncLoadLevelQueue.RemoveAt(0);
		if(Task.FinishDelayTime > 0.f)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &USceneModule::OnAsyncLoadLevelFinished, Task), Task.FinishDelayTime, false);
		}
		else
		{
			OnAsyncLoadLevelFinished(Task);
		}
	}
}

void USceneModule::OnHandleAsyncUnloadLevelFinish()
{
	if(AsyncLoadLevelQueue.IsValidIndex(0))
	{
		FAsyncLoadLevelTask Task = AsyncLoadLevelQueue[0];
		AsyncLoadLevelQueue.RemoveAt(0);
		if(Task.FinishDelayTime > 0.f)
		{
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &USceneModule::OnAsyncUnloadLevelFinished, Task), Task.FinishDelayTime, false);
		}
		else
		{
			OnAsyncUnloadLevelFinished(Task);
		}
	}
}
