#include "Setting/Provider/AudioSettingProvider.h"

#include "Audio/AudioModule.h"

namespace
{
	const FName AudioProviderName(TEXT("Audio"));
	const FName MasterVolumeId(TEXT("Audio.Master.Volume"));
	const FName BackgroundVolumeId(TEXT("Audio.Background.Volume"));
	const FName EnvironmentVolumeId(TEXT("Audio.Environment.Volume"));
	const FName EffectVolumeId(TEXT("Audio.Effect.Volume"));

	FSettingDefinition MakeAudioVolumeDefinition(FName InId, const FText& InDisplayName, int32 InOrder)
	{
		FSettingDefinition Definition;
		Definition.SettingId = FSettingId(InId);
		Definition.Provider = AudioProviderName;
		Definition.Page = AudioProviderName;
		Definition.Category = FName(TEXT("Volume"));
		Definition.Order = InOrder;
		Definition.DisplayName = InDisplayName;
		Definition.Renderer = ESettingRendererType::Number;
		Definition.ApplyPolicy = ESettingApplyPolicy::Deferred;
		Definition.NumberDisplay.bHasMin = true;
		Definition.NumberDisplay.Min = 0.0;
		Definition.NumberDisplay.bHasMax = true;
		Definition.NumberDisplay.Max = 1.0;
		Definition.NumberDisplay.Step = 0.01;
		Definition.NumberDisplay.Scale = 100.0;
		Definition.NumberDisplay.DecimalPlaces = 0;
		Definition.NumberDisplay.Suffix = FText::FromString(TEXT("%"));
		return Definition;
	}
}

void UAudioSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	OutDefinitions.Add(MakeAudioVolumeDefinition(MasterVolumeId, NSLOCTEXT("WH.Setting", "MasterVolume", "Master Volume"), 0));
	OutDefinitions.Add(MakeAudioVolumeDefinition(BackgroundVolumeId, NSLOCTEXT("WH.Setting", "BackgroundVolume", "Background Volume"), 1));
	OutDefinitions.Add(MakeAudioVolumeDefinition(EnvironmentVolumeId, NSLOCTEXT("WH.Setting", "EnvironmentVolume", "Environment Volume"), 2));
	OutDefinitions.Add(MakeAudioVolumeDefinition(EffectVolumeId, NSLOCTEXT("WH.Setting", "EffectVolume", "Effects Volume"), 3));
}

bool UAudioSettingProvider::CanHandle(const FSettingDefinition& InDefinition) const
{
	return InDefinition.Provider == AudioProviderName;
}

void UAudioSettingProvider::BeginEdit(const TArray<FSettingDefinition>& InDefinitions)
{
	Super::BeginEdit(InDefinitions);
	AppliedData = UAudioModule::Get().GetSaveData(true).GetRef<FAudioModuleSaveData>();
	PendingData = AppliedData;
	DefaultData = FAudioModuleSaveData();
}

FParameter UAudioSettingProvider::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	return FParameter(ResolveVolume(AppliedData, InDefinition.SettingId.Name));
}

FParameter UAudioSettingProvider::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	return FParameter(ResolveVolume(PendingData, InDefinition.SettingId.Name));
}

FParameter UAudioSettingProvider::GetDefaultValue(const FSettingDefinition& InDefinition) const
{
	return FParameter(ResolveVolume(DefaultData, InDefinition.SettingId.Name));
}

bool UAudioSettingProvider::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	float Value = 0.f;
	if(!CanHandle(InDefinition) || !InValue.TryGet(Value))
	{
		return false;
	}
	ResolveVolume(PendingData, InDefinition.SettingId.Name) = FMath::Clamp(Value, 0.f, 1.f);
	return true;
}

bool UAudioSettingProvider::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	float Value = 0.f;
	if(!InValue.TryGet(Value))
	{
		return false;
	}

	if(InDefinition.SettingId.Name == MasterVolumeId)
	{
		UAudioModule::Get().SetGlobalSoundVolume(Value);
	}
	else if(InDefinition.SettingId.Name == BackgroundVolumeId)
	{
		UAudioModule::Get().SetBackgroundSoundVolume(Value);
	}
	else if(InDefinition.SettingId.Name == EnvironmentVolumeId)
	{
		UAudioModule::Get().SetEnvironmentSoundVolume(Value);
	}
	else if(InDefinition.SettingId.Name == EffectVolumeId)
	{
		UAudioModule::Get().SetEffectSoundVolume(Value);
	}
	else
	{
		return false;
	}
	return true;
}

void UAudioSettingProvider::Rollback(const FSettingDefinition& InDefinition)
{
	const FParameter AppliedValue(ResolveVolume(AppliedData, InDefinition.SettingId.Name));
	Apply(InDefinition, AppliedValue);
	ResolveVolume(PendingData, InDefinition.SettingId.Name) = AppliedValue.Get<float>();
}

void UAudioSettingProvider::Commit()
{
	AppliedData = PendingData;
}

void UAudioSettingProvider::EndEdit()
{
	AppliedData = FAudioModuleSaveData();
	PendingData = FAudioModuleSaveData();
	DefaultData = FAudioModuleSaveData();
}

float& UAudioSettingProvider::ResolveVolume(FAudioModuleSaveData& InData, FName InSettingName) const
{
	if(InSettingName == MasterVolumeId)
	{
		return InData.GlobalSoundParams.Volume;
	}
	if(InSettingName == BackgroundVolumeId)
	{
		return InData.BackgroundSoundParams.Volume;
	}
	if(InSettingName == EnvironmentVolumeId)
	{
		return InData.EnvironmentSoundParams.Volume;
	}
	return InData.EffectSoundParams.Volume;
}

const float& UAudioSettingProvider::ResolveVolume(const FAudioModuleSaveData& InData, FName InSettingName) const
{
	if(InSettingName == MasterVolumeId)
	{
		return InData.GlobalSoundParams.Volume;
	}
	if(InSettingName == BackgroundVolumeId)
	{
		return InData.BackgroundSoundParams.Volume;
	}
	if(InSettingName == EnvironmentVolumeId)
	{
		return InData.EnvironmentSoundParams.Volume;
	}
	return InData.EffectSoundParams.Volume;
}
