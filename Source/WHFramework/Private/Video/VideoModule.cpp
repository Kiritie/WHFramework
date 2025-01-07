// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModule.h"

#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "MediaHelpers.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "Asset/AssetModuleStatics.h"
#include "Common/CommonStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/VideoSaveGame.h"
#include "Video/VideoModuleNetworkComponent.h"
#include "Video/MediaPlayer/MediaPlayerBase.h"
#include <Windows.h>

IMPLEMENTATION_MODULE(UVideoModule)

// Sets default values
UVideoModule::UVideoModule()
{
	ModuleName = FName("VideoModule");
	ModuleDisplayName = FText::FromString(TEXT("Video Module"));

	ModuleSaveGame = UVideoSaveGame::StaticClass();

	ModuleNetworkComponent = UVideoModuleNetworkComponent::StaticClass();

	MediaPlayers = TArray<AMediaPlayerBase*>();

	WindowMode = EWindowModeN::Fullscreen;
	WindowResolution = EWindowResolution::R_ScreenSize;
	bEnableVSync = false;
	bEnableDynamicResolution = false;
	
	GlobalVideoQuality = EVideoQuality::Epic;
	ViewDistanceQuality = EVideoQuality::Epic;
	ShadowQuality = EVideoQuality::Epic;
	GlobalIlluminationQuality = EVideoQuality::Epic;
	ReflectionQuality = EVideoQuality::Epic;
	AntiAliasingQuality = EVideoQuality::Epic;
	TextureQuality = EVideoQuality::Epic;
	VisualEffectQuality = EVideoQuality::Epic;
	PostProcessingQuality = EVideoQuality::Epic;
	FoliageQuality = EVideoQuality::Epic;
	ShadingQuality = EVideoQuality::Epic;
}

UVideoModule::~UVideoModule()
{
	TERMINATION_MODULE(UVideoModule)
}

#if WITH_EDITOR
void UVideoModule::OnGenerate()
{
	Super::OnGenerate();
}

void UVideoModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UVideoModule)
}
#endif

void UVideoModule::OnInitialize()
{
	Super::OnInitialize();
	
	for(auto Iter : MediaPlayers)
	{
		if(Iter && !MediaPlayerMap.Contains(Iter->GetNameP()))
		{
			MediaPlayerMap.Add(Iter->GetNameP(), Iter);
		}
	}

	UAssetModuleStatics::AddStaticObject(FName("EVideoQuality"), FStaticObject(UEnum::StaticClass(), TEXT("/Script/WHFramework.EVideoQuality")));
}

void UVideoModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		SetWindowMode(WindowMode, false);
		SetWindowResolution(WindowResolution, false);
		SetEnableVSync(bEnableVSync, false);
		SetEnableDynamicResolution(bEnableDynamicResolution, false);
		SetGlobalVideoQuality(GlobalVideoQuality, false);
		SetViewDistanceQuality(ViewDistanceQuality, false);
		SetShadowQuality(ShadowQuality, false);
		SetGlobalIlluminationQuality(GlobalIlluminationQuality, false);
		SetReflectionQuality(ReflectionQuality, false);
		SetAntiAliasingQuality(AntiAliasingQuality, false);
		SetTextureQuality(TextureQuality, false);
		SetVisualEffectQuality(VisualEffectQuality, false);
		SetPostProcessingQuality(PostProcessingQuality, false);
		SetFoliageQuality(FoliageQuality, false);
		SetShadingQuality(ShadingQuality, false);

		ApplyVideoSettings();
	}
}

void UVideoModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UVideoModule::OnPause()
{
	Super::OnPause();
}

void UVideoModule::OnUnPause()
{
	Super::OnUnPause();
}

void UVideoModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UVideoModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FVideoModuleSaveData>();

	if(SaveData.IsSaved())
	{
		SetWindowMode(SaveData.WindowMode, false);
		SetWindowResolution(SaveData.WindowResolution, false);
		SetEnableVSync(SaveData.bEnableVSync, false);
		SetEnableDynamicResolution(SaveData.bEnableDynamicResolution, false);
		SetGlobalVideoQuality(SaveData.GlobalVideoQuality, false);
		SetViewDistanceQuality(SaveData.ViewDistanceQuality, false);
		SetShadowQuality(SaveData.ShadowQuality, false);
		SetGlobalIlluminationQuality(SaveData.GlobalIlluminationQuality, false);
		SetReflectionQuality(SaveData.ReflectionQuality, false);
		SetAntiAliasingQuality(SaveData.AntiAliasingQuality, false);
		SetTextureQuality(SaveData.TextureQuality, false);
		SetVisualEffectQuality(SaveData.VisualEffectQuality, false);
		SetPostProcessingQuality(SaveData.PostProcessingQuality, false);
		SetFoliageQuality(SaveData.FoliageQuality, false);
		SetShadingQuality(SaveData.ShadingQuality, false);

		ApplyVideoSettings();
	}
}

void UVideoModule::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

FSaveData* UVideoModule::ToData()
{
	static FVideoModuleSaveData SaveData;
	SaveData = FVideoModuleSaveData();
	
	SaveData.WindowMode = GetWindowMode();
	SaveData.WindowResolution = GetWindowResolution();
	SaveData.bEnableVSync = IsEnableVSync();
	SaveData.bEnableDynamicResolution = IsEnableDynamicResolution();
	SaveData.GlobalVideoQuality = GetGlobalVideoQuality();
	SaveData.ViewDistanceQuality = GetViewDistanceQuality();
	SaveData.ShadowQuality = GetShadowQuality();
	SaveData.GlobalIlluminationQuality = GetGlobalIlluminationQuality();
	SaveData.ReflectionQuality = GetReflectionQuality();
	SaveData.AntiAliasingQuality = GetAntiAliasingQuality();
	SaveData.TextureQuality = GetTextureQuality();
	SaveData.VisualEffectQuality = GetVisualEffectQuality();
	SaveData.PostProcessingQuality = GetPostProcessingQuality();
	SaveData.FoliageQuality = GetFoliageQuality();
	SaveData.ShadingQuality = GetShadingQuality();

	return &SaveData;
}

void UVideoModule::Load_Implementation()
{
	Super::Load_Implementation();

	if(!bModuleAutoSave)
	{
		SetGlobalVideoQuality(GlobalVideoQuality, true);
	}
}

FString UVideoModule::GetModuleDebugMessage()
{
	FString DebugMessage;
	for(auto Iter : MediaPlayerMap)
	{
		DebugMessage.Appendf(TEXT("%s-%s(%s)\n"), *Iter.Key.ToString(), *Iter.Value->GetMovieName().ToString(), *MediaUtils::StateToString(Iter.Value->GetMediaPlayer()->GetPlayerFacade()->GetPlayer() ? Iter.Value->GetMediaPlayer()->GetPlayerFacade()->GetPlayer()->GetControls().GetState() : EMediaState::Stopped));
	}
	DebugMessage.RemoveFromEnd(TEXT("\n"));
	if(!DebugMessage.IsEmpty())
	{
		return DebugMessage;
	}
	return Super::GetModuleDebugMessage();
}

void UVideoModule::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(!MediaPlayers.Contains(InMediaPlayer))
	{
		MediaPlayers.Add(InMediaPlayer);
	}
	if(!MediaPlayerMap.Contains(InMediaPlayer->GetNameP()))
	{
		MediaPlayerMap.Add(InMediaPlayer->GetNameP(), InMediaPlayer);
	}
}

void UVideoModule::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(MediaPlayers.Contains(InMediaPlayer))
	{
		MediaPlayers.Remove(InMediaPlayer);
	}
	if(MediaPlayerMap.Contains(InMediaPlayer->GetNameP()))
	{
		MediaPlayerMap.Remove(InMediaPlayer->GetNameP());
	}
}

void UVideoModule::RemoveMediaPlayerFromListByName(const FName InName)
{
	RemoveMediaPlayerFromList(GetMediaPlayerByName(InName));
}

AMediaPlayerBase* UVideoModule::GetMediaPlayerByName(const FName InName) const
{
	if(MediaPlayerMap.Contains(InName))
	{
		return MediaPlayerMap[InName];
	}
	return nullptr;
}

void UVideoModule::PlayMovieForMediaPlayer(const FName InName, const FName InMovieName, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PlayMovie(InMovieName, bMulticast);
	}
}

void UVideoModule::PlayMovieForMediaPlayerWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PlayMovieWithDelegate(InMovieName, InOnPlayFinished, bMulticast);
	}
}

void UVideoModule::PauseMovieForMediaPlayer(const FName InName, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PauseMovie(bMulticast);
	}
}

void UVideoModule::SeekMovieForMediaPlayer(const FName InName, const FTimespan& InTimespan, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->SeekMovie(InTimespan, bMulticast);
	}
}

void UVideoModule::StopMovieForMediaPlayer(const FName InName, bool bSkip, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->StopMovie(bSkip, bMulticast);
	}
}

void UVideoModule::ApplyVideoSettings()
{
	GetGameUserSettings()->ApplySettings(true);
}

#if WITH_EDITOR
void UVideoModule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

		bool bApplySetting = UCommonStatics::IsPlaying();
		
		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UVideoModule, bEnableVSync) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UVideoModule, bEnableDynamicResolution))
		{
			bApplySetting = true;
		}
		else if(PropertyName.ToString().EndsWith(TEXT("Quality")))
		{
			const EVideoQuality Quality = *Property->ContainerPtrToValuePtr<EVideoQuality>(this);
			if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UVideoModule, GlobalVideoQuality))
			{
				if(Quality != EVideoQuality::Custom)
				{
					ViewDistanceQuality = Quality;
					ShadowQuality = Quality;
					GlobalIlluminationQuality = Quality;
					ReflectionQuality = Quality;
					AntiAliasingQuality = Quality;
					TextureQuality = Quality;
					VisualEffectQuality = Quality;
					PostProcessingQuality = Quality;
					FoliageQuality = Quality;
					ShadingQuality = Quality;
				}
			}
			else if(Quality != GlobalVideoQuality)
			{
				GlobalVideoQuality = EVideoQuality::Custom;
			}
			bApplySetting = true;
		}
		if(bApplySetting)
		{
			ApplyVideoSettings();
		}
	}
}
#endif

void UVideoModule::SetWindowMode(EWindowModeN InMode, bool bApply)
{
	WindowMode = InMode;
	GetGameUserSettings()->SetFullscreenMode((EWindowMode::Type)InMode);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetWindowResolution(EWindowResolution InResolution, bool bApply)
{
	WindowResolution = InResolution;
	switch (InResolution)
	{
		case EWindowResolution::R_ScreenSize:
		{
			GetGameUserSettings()->SetScreenResolution(GetDesktopResolution());
			break;
		}
		case EWindowResolution::R_3840_2160:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(3840, 2160));
			break;
		}
		case EWindowResolution::R_2560_1440:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(2560, 1440));
			break;
		}
		case EWindowResolution::R_1920_1080:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(1920, 1080));
			break;
		}
		case EWindowResolution::R_1600_900:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(1600, 900));
			break;
		}
		case EWindowResolution::R_1366_768:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(1366, 768));
			break;
		}
		case EWindowResolution::R_1280_720:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(1280, 720));
			break;
		}
		case EWindowResolution::R_1024_576:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(1024, 576));
			break;
		}
		case EWindowResolution::R_960_540:
		{
			GetGameUserSettings()->SetScreenResolution(FIntPoint(960, 540));
			break;
		}
		default: break;
	}
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

FIntPoint UVideoModule::GetDesktopResolution() const
{
	return GetGameUserSettings()->GetDesktopResolution();
}

void UVideoModule::SetEnableVSync(bool bInValue, bool bApply)
{
	bEnableVSync = bInValue;
	GetGameUserSettings()->SetVSyncEnabled(bInValue);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetEnableDynamicResolution(bool bInValue, bool bApply)
{
	bEnableDynamicResolution = bInValue;
	GetGameUserSettings()->SetDynamicResolutionEnabled(bInValue);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetGlobalVideoQuality(EVideoQuality InQuality, bool bApply)
{
	GlobalVideoQuality = InQuality;
	if(InQuality != EVideoQuality::Custom)
	{
		SetViewDistanceQuality(InQuality, false);
		SetShadowQuality(InQuality, false);
		SetGlobalIlluminationQuality(InQuality, false);
		SetReflectionQuality(InQuality, false);
		SetAntiAliasingQuality(InQuality, false);
		SetTextureQuality(InQuality, false);
		SetVisualEffectQuality(InQuality, false);
		SetPostProcessingQuality(InQuality, false);
		SetFoliageQuality(InQuality, false);
		SetShadingQuality(InQuality, false);
		if(bApply) GetGameUserSettings()->ApplySettings(false);
	}
}

void UVideoModule::SetViewDistanceQuality(EVideoQuality InQuality, bool bApply)
{
	ViewDistanceQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetViewDistanceQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetShadowQuality(EVideoQuality InQuality, bool bApply)
{
	ShadowQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetShadowQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetGlobalIlluminationQuality(EVideoQuality InQuality, bool bApply)
{
	GlobalIlluminationQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetGlobalIlluminationQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetReflectionQuality(EVideoQuality InQuality, bool bApply)
{
	ReflectionQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetReflectionQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetAntiAliasingQuality(EVideoQuality InQuality, bool bApply)
{
	AntiAliasingQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetAntiAliasingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetTextureQuality(EVideoQuality InQuality, bool bApply)
{
	TextureQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetTextureQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetVisualEffectQuality(EVideoQuality InQuality, bool bApply)
{
	VisualEffectQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetVisualEffectQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetPostProcessingQuality(EVideoQuality InQuality, bool bApply)
{
	PostProcessingQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetPostProcessingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetFoliageQuality(EVideoQuality InQuality, bool bApply)
{
	FoliageQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetFoliageQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetShadingQuality(EVideoQuality InQuality, bool bApply)
{
	ShadingQuality = InQuality;
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetShadingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

UGameUserSettings* UVideoModule::GetGameUserSettings() const
{
	return GEngine->GetGameUserSettings();
}

void UVideoModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UVideoModule, MediaPlayers);
}
