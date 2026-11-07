#include "Camera/CameraModule.h"
#include "WHFrameworkCoreMacros.h"
#include "GameFramework/PlayerController.h"
#include "Common/LifecycleTypes.h"
#include "Camera/CameraConfigTypes.h"
#include "Camera/Actor/CameraRigBase.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Mode/FreeCameraMode.h"
#include "Main/MainModule.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

IMPLEMENTATION_MODULE(UCameraModule)

UCameraModule::UCameraModule()
{
	ModuleName = TEXT("CameraModule");
	ModuleDisplayName = FText::FromString(TEXT("Camera Module"));
	bModuleRequired = true;

	DefaultRigClass = ACameraRigBase::StaticClass();
	DefaultModeClass = UFreeCameraMode::StaticClass();
}

UCameraModule::~UCameraModule()
{
}

#if WITH_EDITOR
void UCameraModule::OnGenerate()
{
	Super::OnGenerate();
}

void UCameraModule::OnDestroy()
{
	Super::OnDestroy();

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

void UCameraModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!IsBusinessAdmissionOpen())
	{
		MarkLoadDataFailed();
		return;
	}
	if (!InSaveData.Is<FCameraModuleSaveData>() || !PHASEC(InPhase, EPhase::Final)) return;
	if (!ValidateLocalProfileData(InSaveData.GetRef<FCameraModuleSaveData>())) return;
	LocalSaveData = InSaveData.GetRef<FCameraModuleSaveData>();
	LocalSaveData.GetDatas().Reset();
	bLocalProfilesLoaded = true;
	if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		for (ULocalPlayer* Player : Instance->GetLocalPlayers()) ApplyLocalCameraProfile(Player);
}

bool UCameraModule::ValidateLocalProfileData(const FCameraModuleSaveData& InData) const
{
	for (const auto& Pair : InData.Profiles)
	{
		const FCameraUserSettings& Settings = Pair.Value;
		if (Pair.Key.IsEmpty() || Pair.Key.Len() > 128 || !FMath::IsFinite(Settings.MovementSensitivity) ||
			!FMath::IsFinite(Settings.HorizontalLookSensitivity) || !FMath::IsFinite(Settings.VerticalLookSensitivity) ||
			!FMath::IsFinite(Settings.HorizontalPanSensitivity) || !FMath::IsFinite(Settings.VerticalPanSensitivity) ||
			!FMath::IsFinite(Settings.ZoomSensitivity) || !FMath::IsFinite(Settings.MovementSmoothing) ||
			!FMath::IsFinite(Settings.RotationSmoothing) || !FMath::IsFinite(Settings.ZoomSmoothing) ||
			Settings.MovementSmoothing < 0.f || Settings.MovementSmoothing > 1.f ||
			Settings.RotationSmoothing < 0.f || Settings.RotationSmoothing > 1.f ||
			Settings.ZoomSmoothing < 0.f || Settings.ZoomSmoothing > 1.f) return false;
	}
	return true;
}

FParameter UCameraModule::ToData()
{
	if (!bLocalProfilesLoaded || CameraContainerProfile.IsNone()) return FParameter();
	if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		for (ULocalPlayer* Player : Instance->GetLocalPlayers())
		{
			const UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(Player);
			if (Local && !Local->CanCaptureSettings()) return FParameter();
			if (Local && Local->GetPlayerController(GetWorld()) &&
				(Local->CameraContainerProfile != CameraContainerProfile || !Local->bCameraProfileApplied)) return FParameter();
			CaptureLocalCameraProfile(Player);
		}
	}
	return FParameter(LocalSaveData);
}

FParameter UCameraModule::GetData() { return ToData(); }

void UCameraModule::InitializeEmptyLocalProfiles()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (bLocalProfilesLoaded || CameraContainerProfile.IsNone()) return;
	LocalSaveData = FCameraModuleSaveData();
	bLocalProfilesLoaded = true;
	if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		for (ULocalPlayer* Player : Instance->GetLocalPlayers()) ApplyLocalCameraProfile(Player);
}

bool UCameraModule::PrepareLocalProfileLoad(FName InProfileName)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (InProfileName.IsNone()) return false;
	const bool bReloadCurrent = bLocalProfilesLoaded && CameraContainerProfile == InProfileName;
	if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		const TArray<ULocalPlayer*> Players = Instance->GetLocalPlayers();
		for (ULocalPlayer* Entry : Players)
		{
			if (UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(Entry); Local && (!Local->RetryPendingSettingRecovery() || Local->HasSettingEdit())) return false;
		}
		for (ULocalPlayer* Entry : Players)
		{
			UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(Entry);
			if (!Local) continue;
			Local->InvalidateSettingEditForProfileChange();
			if (Local->CameraContainerProfile == InProfileName)
			{
				if (bReloadCurrent && !Local->GetInputProfileId().IsEmpty())
				{
					Local->CameraProfileSessions.Remove(InProfileName);
					Local->bCameraProfileApplied = false;
				}
				continue;
			}
			if (!Local->CameraContainerProfile.IsNone() && Local->bCameraProfileApplied)
				Local->CameraProfileSessions.Add(Local->CameraContainerProfile, Local->LocalCameraSettings);
			Local->CameraContainerProfile = InProfileName;
			Local->bCameraProfileApplied = false;
		}
	}
	CameraContainerProfile = InProfileName;
	bLocalProfilesLoaded = false;
	return true;
}

bool UCameraModule::AreLocalProfilesApplied(FName InProfileName) const
{
	if (!CanSaveLocalProfile(InProfileName)) return false;
	const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!Instance) return false;
	for (const ULocalPlayer* Entry : Instance->GetLocalPlayers())
	{
		const UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(Entry);
		if (Local && Local->GetPlayerController(GetWorld()) &&
			(Local->CameraContainerProfile != InProfileName || !Local->bCameraProfileApplied)) return false;
	}
	return true;
}

void UCameraModule::ApplyLocalCameraProfile(ULocalPlayer* InPlayer, bool bAdoptGuest)
{
	if (!IsBusinessAdmissionOpen()) return;
	UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(InPlayer);
	if (!bLocalProfilesLoaded || !Local || Local->HasSettingEdit() || CameraContainerProfile.IsNone()) return;
	if (Local->CameraContainerProfile.IsNone()) Local->CameraContainerProfile = CameraContainerProfile;
	if (Local->CameraContainerProfile != CameraContainerProfile || Local->bCameraProfileApplied) return;
	const FCameraUserSettings* Saved = Local->CameraProfileSessions.Find(CameraContainerProfile);
	if (!Saved && !Local->GetInputProfileId().IsEmpty()) Saved = LocalSaveData.Profiles.Find(Local->GetInputProfileId());
	if (Saved) Local->LocalCameraSettings = *Saved;
	else if (!bAdoptGuest) Local->LocalCameraSettings = FCameraUserSettings();
	Local->bCameraProfileApplied = true;
	if (APlayerController* Player = Local->GetPlayerController(GetWorld()))
		if (ACameraManagerBase* Manager = GetCameraManagerForPlayer(Player)) Manager->ApplyUserSettings(Local->LocalCameraSettings);
}

void UCameraModule::CaptureLocalCameraProfile(ULocalPlayer* InPlayer)
{
	const UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(InPlayer);
	if (bLocalProfilesLoaded && Local && Local->CanCaptureSettings() && Local->CameraContainerProfile == CameraContainerProfile && !Local->GetInputProfileId().IsEmpty() && Local->bCameraProfileApplied)
		LocalSaveData.Profiles.Add(Local->GetInputProfileId(), Local->LocalCameraSettings);
}

FString UCameraModule::GetModuleDebugMessage()
{
	return Super::GetModuleDebugMessage();
}

void UCameraModule::OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
}

ACameraManagerBase* UCameraModule::GetCameraManagerForPlayer(APlayerController* InController) const
{
    return InController && InController->IsLocalController()
        ? Cast<ACameraManagerBase>(InController->PlayerCameraManager) : nullptr;
}

void UCameraModule::RegisterCameraManager(ACameraManagerBase* Manager)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(!Manager)
	{
		return;
	}
	CameraManagers.AddUnique(Manager);
	if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		for (ULocalPlayer* Player : Instance->GetLocalPlayers())
			if (APlayerController* Controller = Player ? Player->GetPlayerController(GetWorld()) : nullptr;
				Controller && Controller->PlayerCameraManager == Manager)
			{
				ApplyLocalCameraProfile(Player);
				Manager->ApplyUserSettings(GetUserSettings(Controller));
				break;
			}
}

void UCameraModule::UnRegisterCameraManager(ACameraManagerBase* Manager)
{
	if(!Manager)
	{
		return;
	}

	CameraManagers.Remove(Manager);
}

void UCameraModule::RegisterShotAnchor(ACameraShotAnchor* Anchor)
{
	if (!IsBusinessAdmissionOpen()) return;
	if(Anchor)
	{
		ShotAnchors.AddUnique(TWeakObjectPtr<ACameraShotAnchor>(Anchor));
	}
}

void UCameraModule::UnregisterShotAnchor(ACameraShotAnchor* Anchor)
{
	ShotAnchors.Remove(TWeakObjectPtr<ACameraShotAnchor>(Anchor));
}

FCameraUserSettings UCameraModule::GetUserSettings(APlayerController* InPlayer) const
{
	const UWHLocalPlayer* Local = ::IsValid(InPlayer) && InPlayer->GetWorld() == GetWorld() && InPlayer->IsLocalController()
		? Cast<UWHLocalPlayer>(InPlayer->GetLocalPlayer()) : nullptr;
	return Local ? Local->LocalCameraSettings : FCameraUserSettings();
}

bool UCameraModule::ApplyUserSettings(APlayerController* InPlayer, const FCameraUserSettings& Settings, const UObject* SettingOwner)
{
	if (!IsBusinessAdmissionOpen()) return false;
	UWHLocalPlayer* Local = ::IsValid(InPlayer) && InPlayer->GetWorld() == GetWorld() && InPlayer->IsLocalController()
		? Cast<UWHLocalPlayer>(InPlayer->GetLocalPlayer()) : nullptr;
	if (!Local) return false;
	if (Local->HasSettingEdit() && !Local->IsSettingEditOwner(SettingOwner)) Local->NoteExternalCameraSettings();
	Local->LocalCameraSettings = Settings;
	if (ACameraManagerBase* Manager = GetCameraManagerForPlayer(InPlayer)) Manager->ApplyUserSettings(Settings);
	return true;
}
