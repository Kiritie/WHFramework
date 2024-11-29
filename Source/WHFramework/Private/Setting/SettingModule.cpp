// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/SettingModule.h"

#include "Audio/AudioModule.h"
#include "Camera/CameraModule.h"
#include "Input/InputModule.h"
#include "Parameter/ParameterModule.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModuleNetworkComponent.h"
#include "Setting/SettingModuleTypes.h"
#include "Video/VideoModule.h"
#include "Widget/WidgetModule.h"

IMPLEMENTATION_MODULE(USettingModule)

// Sets default values
USettingModule::USettingModule()
{
	ModuleName = FName("SettingModule");
	ModuleDisplayName = FText::FromString(TEXT("Setting Module"));

	ModuleSaveGame = USettingSaveGame::StaticClass();
	
	ModuleNetworkComponent = USettingModuleNetworkComponent::StaticClass();
}

USettingModule::~USettingModule()
{
	TERMINATION_MODULE(USettingModule)
}

#if WITH_EDITOR
void USettingModule::OnGenerate()
{
	Super::OnGenerate();

	TERMINATION_MODULE(USettingModule)
}

void USettingModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void USettingModule::OnInitialize()
{
	Super::OnInitialize();
}

void USettingModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void USettingModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void USettingModule::OnPause()
{
	Super::OnPause();
}

void USettingModule::OnUnPause()
{
	Super::OnUnPause();
}

void USettingModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void USettingModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FSettingModuleSaveData>();

	UWidgetModule::Get().LoadSaveData(&SaveData.WidgetData, InPhase);
	UAudioModule::Get().LoadSaveData(&SaveData.AudioData, InPhase);
	UVideoModule::Get().LoadSaveData(&SaveData.VideoData, InPhase);
	UCameraModule::Get().LoadSaveData(&SaveData.CameraData, InPhase);
	UInputModule::Get().LoadSaveData(&SaveData.InputData, InPhase);
	UParameterModule::Get().LoadSaveData(&SaveData.ParameterData, InPhase);
}

void USettingModule::UnloadData(EPhase InPhase)
{
}

FSaveData* USettingModule::ToData()
{
	static FSettingModuleSaveData SaveData;
	SaveData = FSettingModuleSaveData();

	SaveData.WidgetData = UWidgetModule::Get().GetSaveDataRef<FWidgetModuleSaveData>(true);
	SaveData.AudioData = UAudioModule::Get().GetSaveDataRef<FAudioModuleSaveData>(true);
	SaveData.VideoData = UVideoModule::Get().GetSaveDataRef<FVideoModuleSaveData>(true);
	SaveData.CameraData = UCameraModule::Get().GetSaveDataRef<FCameraModuleSaveData>(true);
	SaveData.InputData = UInputModule::Get().GetSaveDataRef<FInputModuleSaveData>(false);
	SaveData.ParameterData = UParameterModule::Get().GetSaveDataRef<FParameterModuleSaveData>(true);
	
	return &SaveData;
}

void USettingModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
