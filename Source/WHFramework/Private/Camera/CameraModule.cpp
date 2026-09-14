#include "Camera/CameraModule.h"
#include "Camera/Actor/CameraRigBase.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FreeCameraMode.h"
#include "Main/MainModule.h"

IMPLEMENTATION_MODULE(UCameraModule)

UCameraModule::UCameraModule()
{
	ModuleName = TEXT("CameraModule");
	ModuleDisplayName = FText::FromString(TEXT("Camera Module"));
	bModuleRequired = true;

	DefaultRigClass = ACameraRigBase::StaticClass();
	DefaultRig = nullptr;
	DefaultModeClass = UFreeCameraMode::StaticClass();
}

UCameraModule::~UCameraModule()
{
	TERMINATION_MODULE(UCameraModule)
}

#if WITH_EDITOR
void UCameraModule::OnGenerate()
{
	Super::OnGenerate();

	TArray<AActor*> ChildActors;
	GetModuleOwner()->GetAttachedActors(ChildActors);

	for(AActor* ChildActor : ChildActors)
	{
		if(ACameraRigBase* CameraRig = Cast<ACameraRigBase>(ChildActor))
		{
			if(!DefaultRig && DefaultRigClass && CameraRig->IsA(DefaultRigClass))
			{
				DefaultRig = CameraRig;
			}
			else
			{
				CameraRig->Destroy();
			}
		}
	}

	if(DefaultRig && (!DefaultRigClass || !DefaultRig->IsA(DefaultRigClass)))
	{
		DefaultRig->Destroy();
		DefaultRig = nullptr;
	}

	if(!DefaultRig && DefaultRigClass)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		DefaultRig = GetWorld()->SpawnActor<ACameraRigBase>(DefaultRigClass, ActorSpawnParameters);
		if(DefaultRig)
		{
			DefaultRig->SetActorLabel(DefaultRigClass->GetName());
			DefaultRig->AttachToActor(GetModuleOwner(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	Modify();
}

void UCameraModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UCameraModule)

	if(DefaultRig)
	{
		DefaultRig->Destroy();
		DefaultRig = nullptr;
	}
}
#endif

void UCameraModule::OnInitialize()
{
	Super::OnInitialize();

	IDebuggerInterface::Register();
}

void UCameraModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UCameraModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UCameraModule::OnPause()
{
	Super::OnPause();
}

void UCameraModule::OnUnPause()
{
	Super::OnUnPause();
}

void UCameraModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		IDebuggerInterface::UnRegister();
	}
}

void UCameraModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	if(InSaveData)
	{
		ApplyUserSettings(InSaveData->CastRef<FCameraModuleSaveData>().ToUserSettings());
	}
}

FString UCameraModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

void UCameraModule::OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
}

ACameraManagerBase* UCameraModule::GetCameraManager(int32 Index) const
{
	return CameraManagers.IsValidIndex(Index) ? CameraManagers[Index] : nullptr;
}

void UCameraModule::RegisterCameraManager(ACameraManagerBase* Manager)
{
	if(!Manager || Manager->GetLocalPlayerIndex() == INDEX_NONE)
	{
		return;
	}

	const int32 Index = Manager->GetLocalPlayerIndex();
	if(CameraManagers.Num() <= Index)
	{
		CameraManagers.SetNum(Index + 1);
	}

	CameraManagers[Index] = Manager;
	Manager->ApplyUserSettings(UserSettings);
}

void UCameraModule::UnRegisterCameraManager(ACameraManagerBase* Manager)
{
	if(!Manager)
	{
		return;
	}

	const int32 Index = Manager->GetLocalPlayerIndex();
	if(CameraManagers.IsValidIndex(Index) && CameraManagers[Index] == Manager)
	{
		CameraManagers[Index] = nullptr;
	}
}

void UCameraModule::RegisterShotAnchor(ACameraShotAnchor* Anchor)
{
	if(Anchor)
	{
		ShotAnchors.AddUnique(TWeakObjectPtr<ACameraShotAnchor>(Anchor));
	}
}

void UCameraModule::UnregisterShotAnchor(ACameraShotAnchor* Anchor)
{
	ShotAnchors.Remove(TWeakObjectPtr<ACameraShotAnchor>(Anchor));
}

void UCameraModule::ApplyUserSettings(const FCameraUserSettings& Settings)
{
	UserSettings = Settings;

	for(ACameraManagerBase* Manager : CameraManagers)
	{
		if(Manager)
		{
			Manager->ApplyUserSettings(Settings);
		}
	}
}
