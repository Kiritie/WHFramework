
#include "Scene/SceneModule.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Common/CommonStatics.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Debug/DebugModuleTypes.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TargetPoint.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Scene/EventHandle_AsyncLoadLevelFinished.h"
#include "Event/Handle/Scene/EventHandle_AsyncUnloadLevelFinished.h"
#include "Event/Handle/Scene/EventHandle_SetDataLayerOwnerPlayer.h"
#include "Event/Handle/Scene/EventHandle_SetDataLayerRuntimeState.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Main/MainModule.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SaveGame/Module/SceneSaveGame.h"
#include "Scene/Object/WorldTimer.h"
#include "Scene/Object/WorldWeather.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"
#include "Scene/Capture/MiniMapCapture.h"
#include "Scene/Widget/WidgetLoadingLevelPanel.h"
#include "Scene/Widget/WidgetWorldText.h"
#include "Widget/WidgetModuleStatics.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

IMPLEMENTATION_MODULE(USceneModule)

USceneModule::USceneModule()
{
	ModuleName = FName("SceneModule");
	ModuleDisplayName = FText::FromString(TEXT("Scene Module"));

	bModuleRequired = true;

	ModuleSaveGame = USceneSaveGame::StaticClass();

	SeaLevel = 0.f;
	Altitude = 0.f;

	MiniMapCapture = nullptr;

	bMiniMapRotatable = false;

	MiniMapMode = EWorldMiniMapMode::None;
	MiniMapPoint = FTransform::Identity;
	MiniMapRange = 512.f;
	MiniMapMinRange = 128.f;
	MiniMapMaxRange = -1.f;
	
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> MiniMapTexFinder(TEXT("/Script/Engine.TextureRenderTarget2D'/WHFramework/Scene/Textures/Render/RT_MiniMap_Default.RT_MiniMap_Default'"));
	if(MiniMapTexFinder.Succeeded())
	{
		MiniMapTexture = MiniMapTexFinder.Object;
	}
	
	WorldTimer = nullptr;
	WorldWeather = nullptr;

	LoadedLevels = TMap<FName, TSoftObjectPtr<UWorld>>();

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

	UEventModuleStatics::SubscribeEvent<UEventHandle_SetDataLayerRuntimeState>(this, FName("OnSetDataLayerRuntimeState"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_SetDataLayerOwnerPlayer>(this, FName("OnSetDataLayerOwnerPlayer"));

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
			MiniMapCapture->GetCapture()->SetActive(MiniMapMode != EWorldMiniMapMode::None, true);
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

void USceneModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
	
	Altitude = UCameraModuleStatics::GetCameraLocation(true).Z - SeaLevel;

	if(MiniMapCapture)
	{
		switch(MiniMapMode)
		{
			case EWorldMiniMapMode::FixedPoint:
			{
				MiniMapCapture->SetActorLocationAndRotation(MiniMapPoint.GetLocation(), bMiniMapRotatable ? FRotator(0.f, MiniMapPoint.GetRotation().Z, 0.f) : FRotator::ZeroRotator);
				break;
			}
			case EWorldMiniMapMode::ViewPoint:
			{
				if(const AActor* ViewTarget = UCommonStatics::GetPlayerController()->GetViewTarget())
				{
					MiniMapCapture->SetActorLocationAndRotation(ViewTarget->GetActorLocation(), bMiniMapRotatable ? FRotator(0.f, ViewTarget->GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator);
				}
				break;
			}
			case EWorldMiniMapMode::CameraPoint:
			{
				if(const ACameraActorBase* CameraActor = UCameraModuleStatics::GetCurrentCamera())
				{
					MiniMapCapture->SetActorLocationAndRotation(CameraActor->GetActorLocation(), bMiniMapRotatable ? FRotator(0.f, CameraActor->GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator);
				}
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

	for(const auto& Iter : DataLayerPlayerMappings)
	{
		UDataLayerAsset* DataLayer = Iter.Key;
		const int32 PlayerIndex = Iter.Value;
		TArray<AActor*> Actors = UCommonStatics::GetAllActorsOfDataLayer(DataLayer);
		const AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController(PlayerIndex);
		for(const auto Iter1 : Actors)
		{
			Iter1->SetOwner(PlayerController->GetViewTarget());
			TArray<UPrimitiveComponent*> Components;
			Iter1->GetComponents<UPrimitiveComponent>(Components);
			for(const auto Iter2 : Components)
			{
				Iter2->SetOnlyOwnerSee(true);
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
}

void USceneModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FSceneModuleSaveData>();

	if(PHASEC(InPhase, EPhase::All))
	{
		if(SaveData.IsSaved())
		{
			MiniMapRange = SaveData.MiniMapRange;
		}
		if(WorldTimer && WorldTimer->IsAutoSave())
		{
			WorldTimer->LoadSaveData(&SaveData.TimerData);
		}
	
		if(WorldWeather && WorldWeather->IsAutoSave())
		{
			WorldWeather->LoadSaveData(&SaveData.WeatherData);
		}
	}
}

FSaveData* USceneModule::ToData()
{
	static FSceneModuleSaveData* SaveData;
	SaveData = new FSceneModuleSaveData();

	SaveData->MiniMapRange = MiniMapRange;
	
	if(WorldTimer && WorldTimer->IsAutoSave())
	{
		SaveData->TimerData = WorldTimer->GetSaveDataRef<FWorldTimerSaveData>(true);
	}
	
	if(WorldWeather && WorldWeather->IsAutoSave())
	{
		SaveData->WeatherData = WorldWeather->GetSaveDataRef<FWorldWeatherSaveData>(true);
	}

	return SaveData;
}

#if WITH_EDITOR
bool USceneModule::CanEditChange(const FProperty* InProperty) const
{
	if(InProperty)
	{
		const FString PropertyName = InProperty->GetName();

		return true;
	}

	return Super::CanEditChange(InProperty);
}

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

float USceneModule::GetAltitude(bool bUnsigned) const
{
	return bUnsigned ? FMath::Max(Altitude, 0.f) : Altitude;
}

void USceneModule::SetMiniMapMode(EWorldMiniMapMode InMiniMapMode)
{
	MiniMapMode = InMiniMapMode;
	if(MiniMapCapture)
	{
		MiniMapCapture->GetCapture()->SetActive(MiniMapMode != EWorldMiniMapMode::None, true);
	}
}

void USceneModule::SetMiniMapTexture(UTextureRenderTarget2D* InMiniMapTexture)
{
	MiniMapTexture = InMiniMapTexture;

	MiniMapCapture->GetCapture()->TextureTarget = MiniMapTexture;
}

UWorldTimer* USceneModule::GetWorldTimer(TSubclassOf<UWorldTimer> InClass) const
{
	return GetDeterminesOutputObject(WorldTimer, InClass);
}

UWorldWeather* USceneModule::GetWorldWeather(TSubclassOf<UWorldWeather> InClass) const
{
	return GetDeterminesOutputObject(WorldWeather, InClass);
}

void USceneModule::OnSetDataLayerRuntimeState(UObject* InSender, UEventHandle_SetDataLayerRuntimeState* InEventHandle)
{
	UDataLayerManager::GetDataLayerManager(this)->SetDataLayerRuntimeState(InEventHandle->DataLayer, InEventHandle->State, InEventHandle->bRecursive);
}

void USceneModule::OnSetDataLayerOwnerPlayer(UObject* InSender, UEventHandle_SetDataLayerOwnerPlayer* InEventHandle)
{
	if(InEventHandle->PlayerIndex != -1)
	{
		DataLayerPlayerMappings.Emplace(InEventHandle->DataLayer, InEventHandle->PlayerIndex);
	}
	else if(DataLayerPlayerMappings.Contains(InEventHandle->DataLayer))
	{
		DataLayerPlayerMappings.Remove(InEventHandle->DataLayer);
	}
	// const AWHPlayerController* PlayerController = UCommonStatics::GetPlayerController(InEventHandle->PlayerIndex);
	// for(const auto Iter1 : UCommonStatics::GetAllActorsOfDataLayer(InEventHandle->DataLayer))
	// {
	// 	Iter1->SetOwner(PlayerController ? PlayerController->GetViewTarget() : nullptr);
	// 	TArray<UPrimitiveComponent*> Components;
	// 	Iter1->GetComponents<UPrimitiveComponent>(Components);
	// 	for(const auto Iter2 : Components)
	// 	{
	// 		Iter2->SetOnlyOwnerSee(PlayerController != nullptr);
	// 	}
	// }
}

bool USceneModule::HasSceneActor(const FString& InID, bool bEnsured) const
{
	if(SceneActorMap.Contains(FGuid(InID))) return true;
	ensureEditor(!bEnsured);
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
		return true;
	}
	return false;
}

bool USceneModule::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>()) return false;

	if(SceneActorMap.Contains(ISceneActorInterface::Execute_GetActorID(InActor)))
	{
		SceneActorMap.Remove(ISceneActorInterface::Execute_GetActorID(InActor));
		return true;
	}
	return false;
}

bool USceneModule::HasTargetPointByName(FName InName, bool bEnsured) const
{
	if(TargetPoints.Contains(InName)) return true;
	ensureEditor(!bEnsured);
	return false;
}

ATargetPoint* USceneModule::GetTargetPointByName(FName InName, bool bEnsured) const
{
	if(HasTargetPointByName(InName, bEnsured))
	{
		return TargetPoints[InName];
	}
	return nullptr;
}

void USceneModule::AddTargetPointByName(FName InName, ATargetPoint* InPoint)
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

void USceneModule::RemoveTargetPointByName(FName InName)
{
	if(TargetPoints.Contains(InName))
	{
		TargetPoints.Remove(InName);
	}
}

bool USceneModule::HasScenePointByName(FName InName, bool bEnsured) const
{
	if(ScenePoints.Contains(InName)) return true;
	ensureEditor(!bEnsured);
	return false;
}

USceneComponent* USceneModule::GetScenePointByName(FName InName, bool bEnsured) const
{
	if(HasScenePointByName(InName, bEnsured))
	{
		return ScenePoints[InName];
	}
	return nullptr;
}

void USceneModule::AddScenePointByName(FName InName, USceneComponent* InSceneComp)
{
	if(!ScenePoints.Contains(InName))
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

void USceneModule::RemoveScenePointByName(FName InName)
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

bool USceneModule::HasPhysicsVolumeByName(FName InName, bool bEnsured) const
{
	if(PhysicsVolumes.Contains(InName)) return true;
	ensureEditor(!bEnsured);
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

APhysicsVolumeBase* USceneModule::GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
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

void USceneModule::AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume)
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

void USceneModule::RemovePhysicsVolumeByName(FName InName)
{
	if(PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Remove(InName);
	}
}

void USceneModule::SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FWorldWidgetMapping InMapping, FVector InOffsetRange)
{
	if(InOffsetRange != FVector::ZeroVector)
	{
		InMapping.Location = InMapping.Location + FMath::RandPointInBox(FBox(-InOffsetRange * 0.5f, InOffsetRange * 0.5f));
	}
	UWidgetModuleStatics::CreateWorldWidget<UWidgetWorldText>(nullptr, InMapping, { InText, InTextColor, (int32)InTextStyle });
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

void USceneModule::AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	const FString LoadPackagePath = FPaths::GetBaseFilename(InLevelPath.ToString(), false);
	if(!LoadedLevels.Contains(InLevelPath))
	{
		if(bCreateLoadingWidget)
		{
			TArray<FParameter> Parameters { InLevelPath.ToString(), false };
			UWidgetModuleStatics::OpenUserWidget<UWidgetLoadingLevelPanel>(&Parameters);
		}
		LoadPackageAsync(LoadPackagePath, FLoadPackageAsyncDelegate::CreateLambda([&](const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result){
			WHLog(TEXT("Start load level: %s") + LoadPackagePath, EDC_Scene);
			if(Result == EAsyncLoadingResult::Failed)
			{
				WHLog(TEXT("Load level failed!"));
			}
			else if(Result == EAsyncLoadingResult::Succeeded)
			{
				FLatentActionInfo LatentActionInfo;
				UGameplayStatics::LoadStreamLevelBySoftObjectPtr(this, LoadedPackage, true, false, LatentActionInfo);
				if(InFinishDelayTime > 0.f)
				{
					FTimerHandle TimerHandle;
					FTimerDelegate TimerDelegate;
					TimerDelegate.BindUObject(this, &USceneModule::OnAsyncLoadLevelFinished, InLevelPath, InOnAsyncLoadLevelFinished);
					GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, InFinishDelayTime, false);
				}
				else
				{
					OnAsyncLoadLevelFinished(InLevelPath, InOnAsyncLoadLevelFinished);
				}
				if(bCreateLoadingWidget && UWidgetModuleStatics::GetUserWidget<UWidgetLoadingLevelPanel>())
				{
					UWidgetModuleStatics::GetUserWidget<UWidgetLoadingLevelPanel>()->SetLoadProgress(1.f);
				}
				LoadedLevels.Add(InLevelPath, LoadedPackage);
			}
		}), 0, PKG_ContainsMap);
	}
	else
	{
		FLatentActionInfo LatentActionInfo;
		UGameplayStatics::LoadStreamLevelBySoftObjectPtr(this, LoadedLevels[InLevelPath], true, false, LatentActionInfo);
		OnAsyncLoadLevelFinished(InLevelPath, InOnAsyncLoadLevelFinished);
	}
}

void USceneModule::AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	const FString LoadPackagePath = FPaths::GetBaseFilename(InLevelPath.ToString(), false);
	if(LoadedLevels.Contains(InLevelPath))
	{
		FLatentActionInfo LatentActionInfo;
		UGameplayStatics::UnloadStreamLevel(this, *LoadPackagePath, LatentActionInfo, false);
		if(bCreateLoadingWidget)
		{
			TArray<FParameter> Parameters { InLevelPath.ToString(), true };
			UWidgetModuleStatics::OpenUserWidget<UWidgetLoadingLevelPanel>(&Parameters);
		}
		WHLog(TEXT("Start unload level: ") + LoadPackagePath);
		if(InFinishDelayTime > 0.f)
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &USceneModule::OnAsyncUnloadLevelFinished, InLevelPath, InOnAsyncUnloadLevelFinished);
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, InFinishDelayTime, false);
		}
		else
		{
			OnAsyncUnloadLevelFinished(InLevelPath, InOnAsyncUnloadLevelFinished);
		}
		if(bCreateLoadingWidget && UWidgetModuleStatics::GetUserWidget<UWidgetLoadingLevelPanel>())
		{
			UWidgetModuleStatics::GetUserWidget<UWidgetLoadingLevelPanel>()->SetLoadProgress(1.f);
		}
		LoadedLevels.Remove(InLevelPath);
	}
}

float USceneModule::GetAsyncLoadLevelProgress(FName InLevelPath) const
{
	const FString LoadPackagePath = FPaths::GetBaseFilename(InLevelPath.ToString(), false);
	return GetAsyncLoadPercentage(*LoadPackagePath) / 100.f;
}

float USceneModule::GetAsyncUnloadLevelProgress(FName InLevelPath) const
{
	return 1.f;
}

void USceneModule::OnAsyncLoadLevelFinished(FName InLevelPath, const FOnAsyncLoadLevelFinished InOnAsyncLoadLevelFinished)
{
	WHLog(TEXT("Load level Succeeded!"));

	UWidgetModuleStatics::CloseUserWidget<UWidgetLoadingLevelPanel>();

	if(InOnAsyncLoadLevelFinished.IsBound())
	{
		InOnAsyncLoadLevelFinished.Execute(InLevelPath);
	}
	UEventModuleStatics::BroadcastEvent(UEventHandle_AsyncLoadLevelFinished::StaticClass(), this, { InLevelPath.ToString() }, EEventNetType::Multicast);
}

void USceneModule::OnAsyncUnloadLevelFinished(FName InLevelPath, const FOnAsyncUnloadLevelFinished InOnAsyncUnloadLevelFinished)
{
	WHLog(TEXT("Unload level Succeeded!"));

	UWidgetModuleStatics::CloseUserWidget<UWidgetLoadingLevelPanel>();
	
	if(InOnAsyncUnloadLevelFinished.IsBound())
	{
		InOnAsyncUnloadLevelFinished.Execute(InLevelPath);
	}
	UEventModuleStatics::BroadcastEvent(UEventHandle_AsyncUnloadLevelFinished::StaticClass(), this, { InLevelPath.ToString() }, EEventNetType::Multicast);
}
