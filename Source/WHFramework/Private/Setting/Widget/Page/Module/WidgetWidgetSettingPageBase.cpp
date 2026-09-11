// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetWidgetSettingPageBase.h"

#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"
#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetWidgetSettingPageBase::UWidgetWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Title = NSLOCTEXT("WH.WidgetWidgetSettingPageBase", "Interface", "界面");
}

void UWidgetWidgetSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	TArray<FString> LanguageNames;
	ITER_ARRAY(UWidgetModuleStatics::GetWidgetLanguageTypes(), Item, LanguageNames.Add(Item.DisplayName); )
	SettingItem_LanguageType = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(FWidgetEnumSettingItemSpawnParameter(NSLOCTEXT("WH.WidgetWidgetSettingPageBase", "InterfaceLanguage", "界面语言"), LanguageNames), USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_LanguageType->SetValue(UWidgetModuleStatics::GetWidgetLanguageType());
	AddSettingItem(FName("LanguageType"), SettingItem_LanguageType, NSLOCTEXT("WH.WidgetWidgetSettingPageBase", "Global", "全局"));

	SettingItem_GlobalScale = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(FWidgetFloatSettingItemSpawnParameter(NSLOCTEXT("WH.WidgetWidgetSettingPageBase", "UiScale", "界面缩放"), 0.f, 2.f, 0.f, 100.f), USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_GlobalScale->SetValue(UWidgetModuleStatics::GetWidgetGlobalScale());
	AddSettingItem(FName("GlobalScale"), SettingItem_GlobalScale, NSLOCTEXT("WH.WidgetWidgetSettingPageBase", "Global", "全局"));
}

void UWidgetWidgetSettingPageBase::OnApply()
{
	Super::OnApply();

	UWidgetModuleStatics::SetWidgetLanguageType(SettingItem_LanguageType->GetValue().Get<int32>());
	UWidgetModuleStatics::SetWidgetGlobalScale(SettingItem_GlobalScale->GetValue().Get<float>());
}

void UWidgetWidgetSettingPageBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UWidgetWidgetSettingPageBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	SettingItem_LanguageType->SetValue(UWidgetModuleStatics::GetWidgetLanguageType());
	SettingItem_GlobalScale->SetValue(UWidgetModuleStatics::GetWidgetGlobalScale());
}

void UWidgetWidgetSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

	SettingItem_LanguageType->SetValue(GetDefaultSaveData()->CastRef<FWidgetModuleSaveData>().LanguageType);
	SettingItem_GlobalScale->SetValue(GetDefaultSaveData()->CastRef<FWidgetModuleSaveData>().GlobalScale);
}

bool UWidgetWidgetSettingPageBase::CanApply_Implementation() const
{
	return UWidgetModuleStatics::GetWidgetLanguageType() != SettingItem_LanguageType->GetValue().Get<int32>() ||
		UWidgetModuleStatics::GetWidgetGlobalScale() != SettingItem_GlobalScale->GetValue().Get<float>();
}

bool UWidgetWidgetSettingPageBase::CanReset_Implementation() const
{
	return UWidgetModuleStatics::GetWidgetLanguageType() != GetDefaultSaveData()->CastRef<FWidgetModuleSaveData>().LanguageType ||
		UWidgetModuleStatics::GetWidgetGlobalScale() != GetDefaultSaveData()->CastRef<FWidgetModuleSaveData>().GlobalScale;
}

FSaveData* UWidgetWidgetSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().WidgetData;
}
