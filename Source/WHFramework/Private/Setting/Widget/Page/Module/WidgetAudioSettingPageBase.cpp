// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetAudioSettingPageBase.h"

#include "Audio/AudioModuleStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetAudioSettingPageBase::UWidgetAudioSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Title = FText::FromString(TEXT("音频"));
}

void UWidgetAudioSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	SettingItem_GlobalSoundVolume = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { FText::FromString(TEXT("音量大小")), 0.f, 1.f, 0, 100.f }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_GlobalSoundVolume->SetValue(UAudioModuleStatics::GetGlobalSoundVolume());
	AddSettingItem(FName("GlobalSoundVolume"), SettingItem_GlobalSoundVolume, FText::FromString(TEXT("全局")));

	SettingItem_BackgroundSoundVolume = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { FText::FromString(TEXT("音量大小")), 0.f, 1.f, 0, 100.f }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_BackgroundSoundVolume->SetValue(UAudioModuleStatics::GetBackgroundSoundVolume());
	AddSettingItem(FName("BackgroundSoundVolume"), SettingItem_BackgroundSoundVolume, FText::FromString(TEXT("背景")));

	SettingItem_EnvironmentSoundVolume = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { FText::FromString(TEXT("音量大小")), 0.f, 1.f, 0, 100.f }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_EnvironmentSoundVolume->SetValue(UAudioModuleStatics::GetEnvironmentSoundVolume());
	AddSettingItem(FName("EnvironmentSoundVolume"), SettingItem_EnvironmentSoundVolume, FText::FromString(TEXT("环境")));

	SettingItem_EffectSoundVolume = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { FText::FromString(TEXT("音量大小")), 0.f, 1.f, 0, 100.f }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_EffectSoundVolume->SetValue(UAudioModuleStatics::GetEffectSoundVolume());
	AddSettingItem(FName("EffectSoundVolume"), SettingItem_EffectSoundVolume, FText::FromString(TEXT("音效")));
}

void UWidgetAudioSettingPageBase::OnApply()
{
	Super::OnApply();

	UAudioModuleStatics::SetGlobalSoundVolume(SettingItem_GlobalSoundVolume->GetValue().GetFloatValue());
	UAudioModuleStatics::SetBackgroundSoundVolume(SettingItem_BackgroundSoundVolume->GetValue().GetFloatValue());
	UAudioModuleStatics::SetEnvironmentSoundVolume(SettingItem_EnvironmentSoundVolume->GetValue().GetFloatValue());
	UAudioModuleStatics::SetEffectSoundVolume(SettingItem_EffectSoundVolume->GetValue().GetFloatValue());
}

void UWidgetAudioSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

	SettingItem_GlobalSoundVolume->SetValue(GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().GlobalSoundParams.Volume);
	SettingItem_BackgroundSoundVolume->SetValue(GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().BackgroundSoundParams.Volume);
	SettingItem_EnvironmentSoundVolume->SetValue(GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().EnvironmentSoundParams.Volume);
	SettingItem_EffectSoundVolume->SetValue(GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().EffectSoundParams.Volume);
}

void UWidgetAudioSettingPageBase::OnActivated()
{
	Super::OnActivated();
}

void UWidgetAudioSettingPageBase::OnDeactivated()
{
	Super::OnDeactivated();

	SettingItem_GlobalSoundVolume->SetValue(UAudioModuleStatics::GetGlobalSoundVolume());
	SettingItem_BackgroundSoundVolume->SetValue(UAudioModuleStatics::GetBackgroundSoundVolume());
	SettingItem_EnvironmentSoundVolume->SetValue(UAudioModuleStatics::GetEnvironmentSoundVolume());
	SettingItem_EffectSoundVolume->SetValue(UAudioModuleStatics::GetEffectSoundVolume());
}

bool UWidgetAudioSettingPageBase::CanApply_Implementation() const
{
	return UAudioModuleStatics::GetGlobalSoundVolume() != SettingItem_GlobalSoundVolume->GetValue().GetFloatValue() ||
		UAudioModuleStatics::GetBackgroundSoundVolume() != SettingItem_BackgroundSoundVolume->GetValue().GetFloatValue() ||
		UAudioModuleStatics::GetEnvironmentSoundVolume() != SettingItem_EnvironmentSoundVolume->GetValue().GetFloatValue() ||
		UAudioModuleStatics::GetEffectSoundVolume() != SettingItem_EffectSoundVolume->GetValue().GetFloatValue();
}

bool UWidgetAudioSettingPageBase::CanReset_Implementation() const
{
	return UAudioModuleStatics::GetGlobalSoundVolume() != GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().GlobalSoundParams.Volume ||
		UAudioModuleStatics::GetBackgroundSoundVolume() != GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().BackgroundSoundParams.Volume ||
		UAudioModuleStatics::GetEnvironmentSoundVolume() != GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().EnvironmentSoundParams.Volume ||
		UAudioModuleStatics::GetEffectSoundVolume() != GetDefaultSaveData()->CastRef<FAudioModuleSaveData>().EffectSoundParams.Volume;
}

FSaveData* UWidgetAudioSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().AudioData;
}
