// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/SettingModule.h"

#include "Audio/AudioModule.h"
#include "Camera/CameraModule.h"
#include "Input/InputModule.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModuleNetworkComponent.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Item/WidgetBoolSettingItemBase.h"
#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"
#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"
#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"
#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"
#include "Setting/Widget/Item/WidgetTextSettingItemBase.h"
#include "Video/VideoModule.h"

IMPLEMENTATION_MODULE(USettingModule)

// Sets default values
USettingModule::USettingModule()
{
	ModuleName = FName("SettingModule");
	ModuleDisplayName = FText::FromString(TEXT("Setting Module"));

	ModuleSaveGame = USettingSaveGame::StaticClass();
	
	ModuleNetworkComponent = USettingModuleNetworkComponent::StaticClass();
	
	static ConstructorHelpers::FClassFinder<UWidgetSettingItemCategoryBase> SettingItemCategoryClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Setting/Blueprints/Widget/Item/WBP_SettingItemCategory.WBP_SettingItemCategory_C'"));
	if(SettingItemCategoryClassHelper.Succeeded())
	{
		SettingItemCategoryClass = SettingItemCategoryClassHelper.Class;
	}

	static ConstructorHelpers::FClassFinder<UWidgetFloatSettingItemBase> FloatSettingItemClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Setting/Blueprints/Widget/Item/WBP_FloatSettingItem.WBP_FloatSettingItem_C'"));
	if(FloatSettingItemClassHelper.Succeeded())
	{
		FloatSettingItemClass = FloatSettingItemClassHelper.Class;
	}
	static ConstructorHelpers::FClassFinder<UWidgetBoolSettingItemBase> BoolSettingItemClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Setting/Blueprints/Widget/Item/WBP_BoolSettingItem.WBP_BoolSettingItem_C'"));
	if(BoolSettingItemClassHelper.Succeeded())
	{
		BoolSettingItemClass = BoolSettingItemClassHelper.Class;
	}
	static ConstructorHelpers::FClassFinder<UWidgetEnumSettingItemBase> EnumSettingItemClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Setting/Blueprints/Widget/Item/WBP_EnumSettingItem.WBP_EnumSettingItem_C'"));
	if(EnumSettingItemClassHelper.Succeeded())
	{
		EnumSettingItemClass = EnumSettingItemClassHelper.Class;
	}
	static ConstructorHelpers::FClassFinder<UWidgetTextSettingItemBase> TextSettingItemClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Setting/Blueprints/Widget/Item/WBP_TextSettingItem.WBP_TextSettingItem_C'"));
	if(TextSettingItemClassHelper.Succeeded())
	{
		TextSettingItemClass = TextSettingItemClassHelper.Class;
	}
	static ConstructorHelpers::FClassFinder<UWidgetKeySettingItemBase> KeySettingItemClassHelper(TEXT("WidgetBlueprintGeneratedClass'/WHFramework/Setting/Blueprints/Widget/Item/WBP_KeySettingItem.WBP_KeySettingItem_C'"));
	if(KeySettingItemClassHelper.Succeeded())
	{
		KeySettingItemClass = KeySettingItemClassHelper.Class;
	}
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

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
	}
}

void USettingModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
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

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void USettingModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FSettingModuleSaveData>();

	UAudioModule::Get().LoadSaveData(&SaveData.AudioData, InPhase);
	UVideoModule::Get().LoadSaveData(&SaveData.VideoData, InPhase);
	UCameraModule::Get().LoadSaveData(&SaveData.CameraData, InPhase);
	UInputModule::Get().LoadSaveData(&SaveData.InputData, InPhase);
}

void USettingModule::UnloadData(EPhase InPhase)
{
}

FSaveData* USettingModule::ToData()
{
	static FSettingModuleSaveData SaveData;
	SaveData = FSettingModuleSaveData();

	SaveData.AudioData = UAudioModule::Get().GetSaveDataRef<FAudioModuleSaveData>(true);
	SaveData.VideoData = UVideoModule::Get().GetSaveDataRef<FVideoModuleSaveData>(true);
	SaveData.CameraData = UCameraModule::Get().GetSaveDataRef<FCameraModuleSaveData>(true);
	SaveData.InputData = UInputModule::Get().GetSaveDataRef<FInputModuleSaveData>(false);
	
	return &SaveData;
}

void USettingModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
