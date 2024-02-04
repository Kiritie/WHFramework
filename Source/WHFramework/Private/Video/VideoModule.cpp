// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModule.h"

#include "Asset/AssetModuleStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/VideoSaveGame.h"
#include "Video/VideoModuleNetworkComponent.h"
#include "Video/MediaPlayer/MediaPlayerBase.h"

IMPLEMENTATION_MODULE(UVideoModule)

// Sets default values
UVideoModule::UVideoModule()
{
	ModuleName = FName("VideoModule");
	ModuleDisplayName = FText::FromString(TEXT("Video Module"));

	ModuleSaveGame = UVideoSaveGame::StaticClass();

	ModuleNetworkComponent = UVideoModuleNetworkComponent::StaticClass();

	MediaPlayers = TArray<AMediaPlayerBase*>();

	GlobalVideoQuality = EVideoQuality::Epic;
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
}

void UVideoModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
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

	ApplyVideoQualitySettings();
}

void UVideoModule::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

FSaveData* UVideoModule::ToData()
{
	static FVideoModuleSaveData SaveData;
	SaveData = FVideoModuleSaveData();
	
	SaveData.GlobalVideoQuality = GetGlobalVideoQuality();
	SaveData.ViewDistanceQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetViewDistanceQuality() : GetGlobalVideoQuality();
	SaveData.ShadowQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetShadowQuality() : GetGlobalVideoQuality();
	SaveData.GlobalIlluminationQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetGlobalIlluminationQuality() : GetGlobalVideoQuality();
	SaveData.ReflectionQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetReflectionQuality() : GetGlobalVideoQuality();
	SaveData.AntiAliasingQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetAntiAliasingQuality() : GetGlobalVideoQuality();
	SaveData.TextureQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetTextureQuality() : GetGlobalVideoQuality();
	SaveData.VisualEffectQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetVisualEffectQuality() : GetGlobalVideoQuality();
	SaveData.PostProcessingQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetPostProcessingQuality() : GetGlobalVideoQuality();
	SaveData.FoliageQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetFoliageQuality() : GetGlobalVideoQuality();
	SaveData.ShadingQuality = GetGlobalVideoQuality() == EVideoQuality::Custom ? GetShadingQuality() : GetGlobalVideoQuality();

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

void UVideoModule::ApplyVideoQualitySettings()
{
	GetGameUserSettings()->ApplySettings(false);
}

void UVideoModule::SetGlobalVideoQuality(EVideoQuality InQuality, bool bApply)
{
	GlobalVideoQuality = InQuality;

	if(GlobalVideoQuality != EVideoQuality::Custom)
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
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetViewDistanceQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetViewDistanceQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetViewDistanceQuality();
}

void UVideoModule::SetShadowQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetShadowQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetShadowQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetShadowQuality();
}

void UVideoModule::SetGlobalIlluminationQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetGlobalIlluminationQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetGlobalIlluminationQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetGlobalIlluminationQuality();
}

void UVideoModule::SetReflectionQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetReflectionQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetReflectionQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetReflectionQuality();
}

void UVideoModule::SetAntiAliasingQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetAntiAliasingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetAntiAliasingQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetAntiAliasingQuality();
}

void UVideoModule::SetTextureQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetTextureQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetTextureQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetTextureQuality();
}

void UVideoModule::SetVisualEffectQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetVisualEffectQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetVisualEffectQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetVisualEffectQuality();
}

void UVideoModule::SetPostProcessingQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetPostProcessingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetPostProcessingQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetPostProcessingQuality();
}

void UVideoModule::SetFoliageQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetFoliageQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetFoliageQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetFoliageQuality();
}

void UVideoModule::SetShadingQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetShadingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality UVideoModule::GetShadingQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetShadingQuality();
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
