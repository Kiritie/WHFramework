
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
#include "Scene/Widget/WidgetWorldText.h"
#include "Widget/WidgetModuleBPLibrary.h"
		
MODULE_INSTANCE_IMPLEMENTATION(ASceneModule)

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
		WorldTimer->UpdateTimer(DeltaSeconds);
	}
	if (WorldWeather)
	{
		WorldWeather->UpdateWeather(DeltaSeconds);
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
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_AsyncLoadLevelFinished::StaticClass(), EEventNetType::Multicast, this, { FParameter::MakeString(InLevelPath.ToString()) });
}

void ASceneModule::OnAsyncUnloadLevelFinished(FName InLevelPath, const FOnAsyncUnloadLevelFinished InOnAsyncUnloadLevelFinished)
{
	UE_LOG(LogTemp, Warning, TEXT("Unload level Succeeded!"));

	UWidgetModuleBPLibrary::CloseUserWidget<UWidgetLoadingLevelPanel>();
	
	if(InOnAsyncUnloadLevelFinished.IsBound())
	{
		InOnAsyncUnloadLevelFinished.Execute(InLevelPath);
	}
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_AsyncUnloadLevelFinished::StaticClass(), EEventNetType::Multicast, this, { FParameter::MakeString(InLevelPath.ToString()) });
}

bool ASceneModule::HasSceneActor(FGuid InID, bool bEnsured) const
{
	if(SceneActorMap.Contains(InID)) return true;
	ensureEditor(!bEnsured);
	return false;
}

AActor* ASceneModule::GetSceneActor(FGuid InID, TSubclassOf<AActor> InClass, bool bEnsured) const
{
	if(HasSceneActor(InID, bEnsured))
	{
		return SceneActorMap[InID];
	}
	return nullptr;
}

void ASceneModule::AddSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || ISceneActorInterface::Execute_GetContainer(InActor) == this) return;

	if(ISceneActorInterface::Execute_GetContainer(InActor))
	{
		ISceneActorInterface::Execute_GetContainer(InActor)->RemoveSceneActor(InActor);
	}

	SceneActorMap.Add(ISceneActorInterface::Execute_GetActorID(InActor), InActor);
	ISceneActorInterface::Execute_SetContainer(InActor, this);
}

void ASceneModule::RemoveSceneActor(AActor* InActor)
{
	if(!InActor || !InActor->Implements<USceneActorInterface>() || !SceneActorMap.Contains(ISceneActorInterface::Execute_GetActorID(InActor))) return;

	SceneActorMap.Remove(ISceneActorInterface::Execute_GetActorID(InActor));
	ISceneActorInterface::Execute_SetContainer(SceneActorMap[ISceneActorInterface::Execute_GetActorID(InActor)], nullptr);
}

bool ASceneModule::HasTargetPointByName(FName InName, bool bEnsured) const
{
	if(TargetPoints.Contains(InName)) return true;
	ensureEditor(!bEnsured);
	return false;
}

ATargetPoint* ASceneModule::GetTargetPointByName(FName InName, bool bEnsured) const
{
	if(HasTargetPointByName(InName, bEnsured))
	{
		return TargetPoints[InName];
	}
	return nullptr;
}

void ASceneModule::AddTargetPointByName(FName InName, ATargetPoint* InPoint)
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

void ASceneModule::RemoveTargetPointByName(FName InName)
{
	if(TargetPoints.Contains(InName))
	{
		TargetPoints.Remove(InName);
	}
}

bool ASceneModule::HasScenePointByName(FName InName, bool bEnsured) const
{
	if(ScenePoints.Contains(InName)) return true;
	ensureEditor(!bEnsured);
	return false;
}

USceneComponent* ASceneModule::GetScenePointByName(FName InName, bool bEnsured) const
{
	if(HasScenePointByName(InName, bEnsured))
	{
		return ScenePoints[InName];
	}
	return nullptr;
}

void ASceneModule::AddScenePointByName(FName InName, USceneComponent* InSceneComp)
{
	if(!ScenePoints.Contains(InName))
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

inline void ASceneModule::RemoveScenePointByName(FName InName)
{
	if(ScenePoints.Contains(InName))
	{
		ScenePoints.Remove(InName);
	}
}

bool ASceneModule::HasPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
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
	ensureEditor(!bEnsured);
	return false;
}

APhysicsVolumeBase* ASceneModule::GetPhysicsVolumeByClass(TSubclassOf<APhysicsVolumeBase> InClass, bool bEnsured) const
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

void ASceneModule::SpawnWorldText(const FString& InText, const FColor& InTextColor, EWorldTextStyle InTextStyle, FVector InLocation, AActor* InOwnerActor, USceneComponent* InSceneComp)
{
	TArray<FParameter> Parameters = { FParameter::MakeString(InText), FParameter::MakeColor(InTextColor), FParameter::MakeInteger((int32)InTextStyle) };
	UWidgetModuleBPLibrary::CreateWorldWidget<UWidgetWorldText>(InOwnerActor, InLocation, InSceneComp, &Parameters);
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
