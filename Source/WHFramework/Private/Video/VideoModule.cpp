// Fill out your copyright notice in the Description page of Project Settings.


#include "Video/VideoModule.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Module/VideoSaveGame.h"
#include "Video/MediaPlayer/MediaPlayerBase.h"

IMPLEMENTATION_MODULE(AVideoModule)

// Sets default values
AVideoModule::AVideoModule()
{
	ModuleName = FName("VideoModule");
	ModuleSaveGame = UVideoSaveGame::StaticClass();

	MediaPlayers = TArray<AMediaPlayerBase*>();

	GlobalVideoQuality = EVideoQuality::Epic;
}

AVideoModule::~AVideoModule()
{
	TERMINATION_MODULE(AVideoModule)
}

#if WITH_EDITOR
void AVideoModule::OnGenerate()
{
	Super::OnGenerate();
}

void AVideoModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AVideoModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	UAssetModuleBPLibrary::AddStaticObject(FName("EVideoQuality"), FStaticObject(UEnum::StaticClass(), TEXT("/Script/WHFramework.EVideoQuality")));
}

void AVideoModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
		else
		{
			SetGlobalVideoQuality(GlobalVideoQuality, true);
		}
	}
}

void AVideoModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AVideoModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AVideoModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AVideoModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void AVideoModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
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

void AVideoModule::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

FSaveData* AVideoModule::ToData()
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

void AVideoModule::AddMediaPlayerToList(AMediaPlayerBase* InMediaPlayer)
{
	if(!MediaPlayers.Contains(InMediaPlayer))
	{
		MediaPlayers.Add(InMediaPlayer);
	}
}

void AVideoModule::RemoveMediaPlayerFromList(AMediaPlayerBase* InMediaPlayer)
{
	if(MediaPlayers.Contains(InMediaPlayer))
	{
		MediaPlayers.Remove(InMediaPlayer);
	}
}

void AVideoModule::RemoveMediaPlayerFromListByName(const FName InName)
{
	RemoveMediaPlayerFromList(GetMediaPlayerByName(InName));
}

AMediaPlayerBase* AVideoModule::GetMediaPlayerByName(const FName InName) const
{
	for (auto Iter : MediaPlayers)
	{
		if(Iter->GetPlayerName() == InName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void AVideoModule::PlayMediaPlayerMovie(const FName InName, const FName InMovieName, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PlayMovie(InMovieName, bMulticast);
	}
}

void AVideoModule::PlayMovieWithDelegate(const FName InName, const FName InMovieName, const FOnMoviePlayFinishedSingleDelegate& InOnPlayFinished, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->PlayMovieWithDelegate(InMovieName, InOnPlayFinished, bMulticast);
	}
}

void AVideoModule::StopMediaPlayerMovie(const FName InName, bool bSkip, bool bMulticast)
{
	if(AMediaPlayerBase* MediaPlayer = GetMediaPlayerByName(InName))
	{
		MediaPlayer->StopMovie(bSkip, bMulticast);
	}
}

void AVideoModule::ApplyVideoQualitySettings()
{
	GetGameUserSettings()->ApplySettings(false);
}

void AVideoModule::SetGlobalVideoQuality(EVideoQuality InQuality, bool bApply)
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

void AVideoModule::SetViewDistanceQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetViewDistanceQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetViewDistanceQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetViewDistanceQuality();
}

void AVideoModule::SetShadowQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetShadowQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetShadowQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetShadowQuality();
}

void AVideoModule::SetGlobalIlluminationQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetGlobalIlluminationQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetGlobalIlluminationQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetGlobalIlluminationQuality();
}

void AVideoModule::SetReflectionQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetReflectionQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetReflectionQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetReflectionQuality();
}

void AVideoModule::SetAntiAliasingQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetAntiAliasingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetAntiAliasingQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetAntiAliasingQuality();
}

void AVideoModule::SetTextureQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetTextureQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetTextureQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetTextureQuality();
}

void AVideoModule::SetVisualEffectQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetVisualEffectQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetVisualEffectQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetVisualEffectQuality();
}

void AVideoModule::SetPostProcessingQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetPostProcessingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetPostProcessingQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetPostProcessingQuality();
}

void AVideoModule::SetFoliageQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetFoliageQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetFoliageQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetFoliageQuality();
}

void AVideoModule::SetShadingQuality(EVideoQuality InQuality, bool bApply)
{
	if(InQuality != GlobalVideoQuality) GlobalVideoQuality = EVideoQuality::Custom;
	GetGameUserSettings()->SetShadingQuality((int32)InQuality);
	if(bApply) GetGameUserSettings()->ApplySettings(false);
}

EVideoQuality AVideoModule::GetShadingQuality() const
{
	return (EVideoQuality)GetGameUserSettings()->GetShadingQuality();
}

UGameUserSettings* AVideoModule::GetGameUserSettings() const
{
	return GEngine->GetGameUserSettings();
}

void AVideoModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVideoModule, MediaPlayers);
}
