
#include "Scene/SceneModule.h"

#include "Components/CanvasPanelSlot.h"
#include "Debug/DebugModuleTypes.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TargetPoint.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Scene/EventHandle_AsyncLoadLevelFinished.h"
#include "Event/Handle/Scene/EventHandle_AsyncUnloadLevelFinished.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Scene/Components/WorldTimerComponent.h"
#include "Scene/Components/WorldWeatherComponent.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"
#include "Scene/Widget/WidgetLoadingLevelPanel.h"
#include "Widget/WidgetModuleBPLibrary.h"

ASceneModule::ASceneModule()
{
	ModuleName = FName("SceneModule");

	static ConstructorHelpers::FClassFinder<UWorldTimerComponent> WorldTimerClass(TEXT("Blueprint'/WHFramework/Scene/Blueprints/Components/BPC_WorldTimer.BPC_WorldTimer_C'"));
	if(WorldTimerClass.Succeeded())
	{
		WorldTimer = Cast<UWorldTimerComponent>(CreateDefaultSubobject(TEXT("WorldTimer"), WorldTimerClass.Class, WorldTimerClass.Class, true, true));
	}
	static ConstructorHelpers::FClassFinder<UWorldWeatherComponent> WeatherTimerClass(TEXT("Blueprint'/WHFramework/Scene/Blueprints/Components/BPC_WorldWeather.BPC_WorldWeather_C'"));
	if(WeatherTimerClass.Succeeded())
	{                                                                                          
		WorldWeather = Cast<UWorldWeatherComponent>(CreateDefaultSubobject(TEXT("WorldWeather"), WeatherTimerClass.Class, WeatherTimerClass.Class, true, true));
	}

	LoadedLevels = TMap<FName, TSoftObjectPtr<UWorld>>();

	SceneActors = TArray<AActor*>();
	SceneActorMap = TMap<FName, AActor*>();

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

	OutlineMat = LoadObject<UMaterialInterface>(nullptr, TEXT("Material'/WHFramework/Scene/Materials/M_PostHigtLight.M_PostHigtLight'"));
	OutlineColor = FLinearColor(0.03f, 0.7f, 1.f);
	OutlineMatInst = nullptr;
}

#if WITH_EDITOR
void ASceneModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ASceneModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ASceneModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

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
}

void ASceneModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

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

void ASceneModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(WorldTimer)
	{
		WorldTimer->UpdateTimer();
	}
	if (WorldWeather)
	{
		WorldWeather->UpdateWeather();
	}
}

void ASceneModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ASceneModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void ASceneModule::AsyncLoadLevel(FName InLevelPath, const FOnAsyncLoadLevelFinished& InOnAsyncLoadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	const FString LoadPackagePath = FPaths::GetBaseFilename(InLevelPath.ToString(), false);
	if(!LoadedLevels.Contains(InLevelPath))
	{
		if(bCreateLoadingWidget)
		{
			TArray<FParameter> Parameters { FParameter::MakeString(InLevelPath.ToString()), FParameter::MakeBoolean(false) };
			UWidgetModuleBPLibrary::OpenUserWidget<UWidgetLoadingLevelPanel>(&Parameters);
		}
		LoadPackageAsync(LoadPackagePath, FLoadPackageAsyncDelegate::CreateLambda([=](const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result){
			UE_LOG(LogTemp, Warning, TEXT("Start load level: %s"), *LoadPackagePath);
			if(Result == EAsyncLoadingResult::Failed)
			{
				UE_LOG(LogTemp, Warning, TEXT("Load level failed!"));
			}
			else if(Result == EAsyncLoadingResult::Succeeded)
			{
				FLatentActionInfo LatentActionInfo;
				UGameplayStatics::LoadStreamLevelBySoftObjectPtr(this, LoadedPackage, true, false, LatentActionInfo);
				if(InFinishDelayTime > 0.f)
				{
					FTimerHandle TimerHandle;
					FTimerDelegate TimerDelegate;
					TimerDelegate.BindUObject(this, &ASceneModule::OnAsyncLoadLevelFinished, InLevelPath, InOnAsyncLoadLevelFinished);
					GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, InFinishDelayTime, false);
				}
				else
				{
					OnAsyncLoadLevelFinished(InLevelPath, InOnAsyncLoadLevelFinished);
				}
				if(bCreateLoadingWidget && UWidgetModuleBPLibrary::GetUserWidget<UWidgetLoadingLevelPanel>())
				{
					UWidgetModuleBPLibrary::GetUserWidget<UWidgetLoadingLevelPanel>()->SetLoadProgress(1.f);
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

void ASceneModule::AsyncUnloadLevel(FName InLevelPath, const FOnAsyncUnloadLevelFinished& InOnAsyncUnloadLevelFinished, float InFinishDelayTime, bool bCreateLoadingWidget)
{
	const FString LoadPackagePath = FPaths::GetBaseFilename(InLevelPath.ToString(), false);
	if(LoadedLevels.Contains(InLevelPath))
	{
		FLatentActionInfo LatentActionInfo;
		UGameplayStatics::UnloadStreamLevel(this, *LoadPackagePath, LatentActionInfo, false);
		if(bCreateLoadingWidget)
		{
			TArray<FParameter> Parameters { FParameter::MakeString(InLevelPath.ToString()), FParameter::MakeBoolean(true) };
			UWidgetModuleBPLibrary::OpenUserWidget<UWidgetLoadingLevelPanel>(&Parameters);
		}
		UE_LOG(LogTemp, Warning, TEXT("Start unload level: %s"), *LoadPackagePath);
		if(InFinishDelayTime > 0.f)
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &ASceneModule::OnAsyncUnloadLevelFinished, InLevelPath, InOnAsyncUnloadLevelFinished);
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, InFinishDelayTime, false);
		}
		else
		{
			OnAsyncUnloadLevelFinished(InLevelPath, InOnAsyncUnloadLevelFinished);
		}
		if(bCreateLoadingWidget && UWidgetModuleBPLibrary::GetUserWidget<UWidgetLoadingLevelPanel>())
		{
			UWidgetModuleBPLibrary::GetUserWidget<UWidgetLoadingLevelPanel>()->SetLoadProgress(1.f);
		}
		LoadedLevels.Remove(InLevelPath);
	}
}

float ASceneModule::GetAsyncLoadLevelProgress(FName InLevelPath) const
{
	const FString LoadPackagePath = FPaths::GetBaseFilename(InLevelPath.ToString(), false);
	return GetAsyncLoadPercentage(*LoadPackagePath) / 100.f;
}

float ASceneModule::GetAsyncUnloadLevelProgress(FName InLevelPath) const
{
	return 1.f;
}

void ASceneModule::OnAsyncLoadLevelFinished(FName InLevelPath, const FOnAsyncLoadLevelFinished InOnAsyncLoadLevelFinished)
{
	UE_LOG(LogTemp, Warning, TEXT("Load level Succeeded!"));

	UWidgetModuleBPLibrary::CloseUserWidget<UWidgetLoadingLevelPanel>();

	if(InOnAsyncLoadLevelFinished.IsBound())
	{
		InOnAsyncLoadLevelFinished.Execute(InLevelPath);
	}
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_AsyncLoadLevelFinished::StaticClass(), EEventNetType::Multicast, this, TArray<FParameter>{ FParameter::MakeString(InLevelPath.ToString()) });
}

void ASceneModule::OnAsyncUnloadLevelFinished(FName InLevelPath, const FOnAsyncUnloadLevelFinished InOnAsyncUnloadLevelFinished)
{
	UE_LOG(LogTemp, Warning, TEXT("Unload level Succeeded!"));

	UWidgetModuleBPLibrary::CloseUserWidget<UWidgetLoadingLevelPanel>();
	
	if(InOnAsyncUnloadLevelFinished.IsBound())
	{
		InOnAsyncUnloadLevelFinished.Execute(InLevelPath);
	}
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_AsyncUnloadLevelFinished::StaticClass(), EEventNetType::Multicast, this, TArray<FParameter>{ FParameter::MakeString(InLevelPath.ToString()) });
}

bool ASceneModule::HasSceneActor(TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(!InClass) return false;
	
	if(InClass->ImplementsInterface(USceneActorInterface::StaticClass()))
	{
		return HasSceneActorByName(ISceneActorInterface::Execute_GetObjectName(InClass->GetDefaultObject()), bEnsured);
	}
	return false;
}

bool ASceneModule::HasSceneActorByName(FName InName, bool bEnsured) const
{
	if(SceneActorMap.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

AActor* ASceneModule::GetSceneActor(TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(!InClass) return nullptr;
	
	if(InClass->ImplementsInterface(USceneActorInterface::StaticClass()))
	{
		return GetSceneActorByName(ISceneActorInterface::Execute_GetObjectName(InClass->GetDefaultObject()), InClass, bEnsured);
	}
	return nullptr;
}

AActor* ASceneModule::GetSceneActorByName(FName InName, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(HasSceneActorByName(InName, bEnsured))
	{
		return SceneActorMap[InName];
	}
	return nullptr;
}

void ASceneModule::AddSceneActor(AActor* InActor)
{
	if(!InActor) return;

	if(InActor->GetClass()->ImplementsInterface(USceneActorInterface::StaticClass()))
	{
		AddSceneActorByName(ISceneActorInterface::Execute_GetObjectName(InActor), InActor);
	}
}

void ASceneModule::AddSceneActorByName(FName InName, AActor* InActor)
{
	if(!InActor) return;

	if(!SceneActorMap.Contains(InName))
	{
		SceneActorMap.Add(InName, InActor);
		AddScenePoint(InName, InActor->GetRootComponent());
	}
}

void ASceneModule::RemoveSceneActor(AActor* InActor)
{
	if(!InActor) return;

	if(InActor->GetClass()->ImplementsInterface(USceneActorInterface::StaticClass()))
	{
		RemoveSceneActorByName(ISceneActorInterface::Execute_GetObjectName(InActor));
	}
}

void ASceneModule::RemoveSceneActorByName(FName InName)
{
	if(SceneActorMap.Contains(InName))
	{
		SceneActorMap.Remove(InName);
		RemoveScenePoint(InName);
	}
}

void ASceneModule::DestroySceneActor(AActor* InActor)
{
	if(!InActor) return;

	if(InActor->GetClass()->ImplementsInterface(USceneActorInterface::StaticClass()))
	{
		DestroySceneActorByName(ISceneActorInterface::Execute_GetObjectName(InActor));
	}
}

void ASceneModule::DestroySceneActorByName(FName InName)
{
	if(SceneActorMap.Contains(InName))
	{
		if(SceneActorMap[InName])
		{
			SceneActorMap[InName]->Destroy();
		}
		SceneActorMap.Remove(InName);
	}
}

bool ASceneModule::HasTargetPoint(FName InName, bool bEnsured) const
{
	if(TargetPoints.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

ATargetPoint* ASceneModule::GetTargetPoint(FName InName, bool bEnsured) const
{
	if(HasTargetPoint(InName, bEnsured))
	{
		return TargetPoints[InName];
	}
	return nullptr;
}

void ASceneModule::AddTargetPoint(FName InName, ATargetPoint* InPoint)
{
	if(!TargetPoints.Contains(InName))
	{
		TargetPoints.Add(InName, InPoint);
	}
	if(InPoint && InPoint->IsValidLowLevel())
	{
		AddScenePoint(InName, InPoint->GetRootComponent());
	}
}

void ASceneModule::RemoveTargetPoint(FName InName)
{
	if(TargetPoints.Contains(InName))
	{
		TargetPoints.Remove(InName);
	}
}

bool ASceneModule::HasScenePoint(FName InName, bool bEnsured) const
{
	if(ScenePoints.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

USceneComponent* ASceneModule::GetScenePoint(FName InName, bool bEnsured) const
{
	if(HasScenePoint(InName, bEnsured))
	{
		return ScenePoints[InName];
	}
	return nullptr;
}

void ASceneModule::AddScenePoint(FName InName, USceneComponent* InSceneComp)
{
	if(!ScenePoints.Contains(InName))
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

inline void ASceneModule::RemoveScenePoint(FName InName)
{
	if(ScenePoints.Contains(InName))
	{
		ScenePoints.Remove(InName);
	}
}

bool ASceneModule::HasPhysicsVolume(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
{
	if(!InClass) return false;

	if(APhysicsVolumeBase* DefaultPhysicsVolume = Cast<APhysicsVolumeBase>(InClass->GetDefaultObject()))
	{
		return HasPhysicsVolumeByName(DefaultPhysicsVolume->GetVolumeName(), bEnsured);
	}
	return false;
}

bool ASceneModule::HasPhysicsVolumeByName(FName InName, bool bEnsured) const
{
	if(PhysicsVolumes.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

APhysicsVolumeBase* ASceneModule::GetPhysicsVolume(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
{
	if(!InClass) return nullptr;

	if(APhysicsVolumeBase* DefaultPhysicsVolume = Cast<APhysicsVolumeBase>(InClass->GetDefaultObject()))
	{
		return GetPhysicsVolumeByName(DefaultPhysicsVolume->GetVolumeName(), InClass, bEnsured);
	}
	return nullptr;
}

APhysicsVolumeBase* ASceneModule::GetPhysicsVolumeByName(FName InName, TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
{
	if(HasPhysicsVolumeByName(InName, bEnsured))
	{
		return PhysicsVolumes[InName];
	}
	return nullptr;
}

void ASceneModule::AddPhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(!InPhysicsVolume) return;
	
	AddPhysicsVolumeByName(InPhysicsVolume->GetVolumeName(), InPhysicsVolume);
}

void ASceneModule::AddPhysicsVolumeByName(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(!InPhysicsVolume) return;
	
	if(!PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Add(InName, InPhysicsVolume);
	}
}

void ASceneModule::RemovePhysicsVolume(APhysicsVolumeBase* InPhysicsVolume)
{
	if(!InPhysicsVolume) return;

	RemovePhysicsVolumeByName(InPhysicsVolume->GetVolumeName());
}

void ASceneModule::RemovePhysicsVolumeByName(FName InName)
{
	if(PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes.Remove(InName);
	}
}

FLinearColor ASceneModule::GetOutlineColor() const
{
	return OutlineColor;
}

void ASceneModule::SetOutlineColor(const FLinearColor& InColor)
{
	OutlineColor = InColor;
	if(OutlineMatInst)
	{
		OutlineMatInst->SetVectorParameterValue("Color", OutlineColor);
	}
}
