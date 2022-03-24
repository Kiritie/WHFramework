
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
#include "Scene/Object/SceneObject.h"
#include "Scene/Object/PhysicsVolume/PhysicsVolumeBase.h"
#include "Scene/Widget/WidgetLoadingLevelPanel.h"
#include "Widget/WidgetModuleBPLibrary.h"

ASceneModule::ASceneModule()
{
	ModuleName = FName("SceneModule");

	LoadedLevels = TMap<FName, TSoftObjectPtr<UWorld>>();

	SceneObjects = TMap<FName, TScriptInterface<ISceneObject>>();

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
			TArray<FParameter> Parameters { FParameter::MakeString(InLevelPath.ToString()) };
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
			TArray<FParameter> Parameters { FParameter::MakeString(InLevelPath.ToString()) };
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

bool ASceneModule::HasSceneObject(FName InName, bool bEnsured) const
{
	if(SceneObjects.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

TScriptInterface<ISceneObject> ASceneModule::GetSceneObject(FName InName, bool bEnsured) const
{
	if(HasSceneObject(InName, bEnsured))
	{
		return SceneObjects[InName];
	}
	return nullptr;
}

void ASceneModule::SetSceneObject(FName InName, TScriptInterface<ISceneObject> InObject)
{
	if(SceneObjects.Contains(InName))
	{
		SceneObjects[InName] = InObject;
	}
	else
	{
		SceneObjects.Add(InName, InObject);
	}
	if(InObject.GetObject() && InObject.GetObject()->IsValidLowLevel())
	{
		SetScenePoint(InName, InObject->Execute_GetScenePoint(InObject.GetObject()));
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

void ASceneModule::SetTargetPoint(FName InName, ATargetPoint* InPoint)
{
	if(TargetPoints.Contains(InName))
	{
		TargetPoints[InName] = InPoint;
	}
	else
	{
		TargetPoints.Add(InName, InPoint);
	}
	if(InPoint && InPoint->IsValidLowLevel())
	{
		SetScenePoint(InName, InPoint->GetRootComponent());
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

void ASceneModule::SetScenePoint(FName InName, USceneComponent* InSceneComp)
{
	if(ScenePoints.Contains(InName))
	{
		ScenePoints[InName] = InSceneComp;
	}
	else
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

bool ASceneModule::HasPhysicsVolume(FName InName, bool bEnsured) const
{
	if(PhysicsVolumes.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

APhysicsVolumeBase* ASceneModule::GetPhysicsVolume(FName InName, bool bEnsured) const
{
	if(HasPhysicsVolume(InName, bEnsured))
	{
		return PhysicsVolumes[InName];
	}
	return nullptr;
}

void ASceneModule::SetPhysicsVolume(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes[InName] = InPhysicsVolume;
	}
	else
	{
		PhysicsVolumes.Add(InName, InPhysicsVolume);
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
