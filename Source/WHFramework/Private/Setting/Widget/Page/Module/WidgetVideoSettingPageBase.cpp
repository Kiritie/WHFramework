// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetVideoSettingPageBase.h"


#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"
#include "Video/VideoModule.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetVideoSettingPageBase::UWidgetVideoSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("VideoSettingPage");

	Title = FText::FromString(TEXT("视频"));
}

void UWidgetVideoSettingPageBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetVideoSettingPageBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	SettingItem_GlobalVideoQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality") }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("GlobalVideoQuality"), SettingItem_GlobalVideoQuality, FText::FromString(TEXT("预设")));

	SettingItem_ViewDistanceQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("ViewDistanceQuality"), SettingItem_ViewDistanceQuality, FText::FromString(TEXT("视距")));

	SettingItem_ShadowQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("ShadowQuality"), SettingItem_ShadowQuality, FText::FromString(TEXT("阴影")));

	SettingItem_GlobalIlluminationQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("GlobalIlluminationQuality"), SettingItem_GlobalIlluminationQuality, FText::FromString(TEXT("光照")));

	SettingItem_ReflectionQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("ReflectionQuality"), SettingItem_ReflectionQuality, FText::FromString(TEXT("反射")));

	SettingItem_AntiAliasingQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("AntiAliasingQuality"), SettingItem_AntiAliasingQuality, FText::FromString(TEXT("抗锯齿")));

	SettingItem_TextureQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("TextureQuality"), SettingItem_TextureQuality, FText::FromString(TEXT("贴图")));

	SettingItem_VisualEffectQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("VisualEffectQuality"), SettingItem_VisualEffectQuality, FText::FromString(TEXT("视效")));

	SettingItem_PostProcessingQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("PostProcessingQuality"), SettingItem_PostProcessingQuality, FText::FromString(TEXT("后处理")));

	SettingItem_FoliageQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("FoliageQuality"), SettingItem_FoliageQuality, FText::FromString(TEXT("植被")));

	SettingItem_ShadingQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { FText::FromString(TEXT("画质等级")), FString("/Script/WHFramework.EVideoQuality"), 5 }, false, USettingModule::Get().GetEnumSettingItemClass());
	AddSettingItem(FName("ShadingQuality"), SettingItem_ShadingQuality, FText::FromString(TEXT("遮蔽")));
}

void UWidgetVideoSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	SettingItem_GlobalVideoQuality->SetValue((int32)UVideoModule::Get().GetGlobalVideoQuality());
	SettingItem_ViewDistanceQuality->SetValue((int32)UVideoModule::Get().GetViewDistanceQuality());
	SettingItem_ShadowQuality->SetValue((int32)UVideoModule::Get().GetShadowQuality());
	SettingItem_GlobalIlluminationQuality->SetValue((int32)UVideoModule::Get().GetGlobalIlluminationQuality());
	SettingItem_ReflectionQuality->SetValue((int32)UVideoModule::Get().GetReflectionQuality());
	SettingItem_AntiAliasingQuality->SetValue((int32)UVideoModule::Get().GetAntiAliasingQuality());
	SettingItem_TextureQuality->SetValue((int32)UVideoModule::Get().GetTextureQuality());
	SettingItem_VisualEffectQuality->SetValue((int32)UVideoModule::Get().GetVisualEffectQuality());
	SettingItem_PostProcessingQuality->SetValue((int32)UVideoModule::Get().GetPostProcessingQuality());
	SettingItem_FoliageQuality->SetValue((int32)UVideoModule::Get().GetFoliageQuality());
	SettingItem_ShadingQuality->SetValue((int32)UVideoModule::Get().GetShadingQuality());
}

void UWidgetVideoSettingPageBase::OnApply()
{
	Super::OnApply();

	UVideoModule::Get().SetGlobalVideoQuality((EVideoQuality)SettingItem_GlobalVideoQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetViewDistanceQuality((EVideoQuality)SettingItem_ViewDistanceQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetShadowQuality((EVideoQuality)SettingItem_ShadowQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetGlobalIlluminationQuality((EVideoQuality)SettingItem_GlobalIlluminationQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetReflectionQuality((EVideoQuality)SettingItem_ReflectionQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetAntiAliasingQuality((EVideoQuality)SettingItem_AntiAliasingQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetTextureQuality((EVideoQuality)SettingItem_TextureQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetVisualEffectQuality((EVideoQuality)SettingItem_VisualEffectQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetPostProcessingQuality((EVideoQuality)SettingItem_PostProcessingQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetFoliageQuality((EVideoQuality)SettingItem_FoliageQuality->GetValue().GetIntegerValue(), false);
	UVideoModule::Get().SetShadingQuality((EVideoQuality)SettingItem_ShadingQuality->GetValue().GetIntegerValue(), false);

	UVideoModule::Get().ApplyVideoQualitySettings();
}

void UWidgetVideoSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

	SettingItem_GlobalVideoQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().GlobalVideoQuality);
	SettingItem_ViewDistanceQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ViewDistanceQuality);
	SettingItem_ShadowQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ShadowQuality);
	SettingItem_GlobalIlluminationQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().GlobalIlluminationQuality);
	SettingItem_ReflectionQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ReflectionQuality);
	SettingItem_AntiAliasingQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().AntiAliasingQuality);
	SettingItem_TextureQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().TextureQuality);
	SettingItem_VisualEffectQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().VisualEffectQuality);
	SettingItem_PostProcessingQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().PostProcessingQuality);
	SettingItem_FoliageQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().FoliageQuality);
	SettingItem_ShadingQuality->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ShadingQuality);
}

void UWidgetVideoSettingPageBase::OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue)
{
	Super::OnValueChange(InSettingItem, InValue);

	if(InSettingItem == SettingItem_GlobalVideoQuality)
	{
		if(InValue.GetIntegerValue() != (int32)EVideoQuality::Custom)
		{
			SettingItem_ViewDistanceQuality->SetValue(InValue);
			SettingItem_ShadowQuality->SetValue(InValue);
			SettingItem_GlobalIlluminationQuality->SetValue(InValue);
			SettingItem_ReflectionQuality->SetValue(InValue);
			SettingItem_AntiAliasingQuality->SetValue(InValue);
			SettingItem_TextureQuality->SetValue(InValue);
			SettingItem_VisualEffectQuality->SetValue(InValue);
			SettingItem_PostProcessingQuality->SetValue(InValue);
			SettingItem_FoliageQuality->SetValue(InValue);
			SettingItem_ShadingQuality->SetValue(InValue);
		}
	}
	else
	{
		if(InValue != SettingItem_GlobalVideoQuality->GetValue())
		{
			SettingItem_GlobalVideoQuality->SetValue((int32)EVideoQuality::Custom);
		}
	}
}

void UWidgetVideoSettingPageBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

bool UWidgetVideoSettingPageBase::CanApply_Implementation() const
{
	return UVideoModule::Get().GetGlobalVideoQuality() != (EVideoQuality)SettingItem_GlobalVideoQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetViewDistanceQuality() != (EVideoQuality)SettingItem_ViewDistanceQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetShadowQuality() != (EVideoQuality)SettingItem_ShadowQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetGlobalIlluminationQuality() != (EVideoQuality)SettingItem_GlobalIlluminationQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetReflectionQuality() != (EVideoQuality)SettingItem_ReflectionQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetAntiAliasingQuality() != (EVideoQuality)SettingItem_AntiAliasingQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetTextureQuality() != (EVideoQuality)SettingItem_TextureQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetVisualEffectQuality() != (EVideoQuality)SettingItem_VisualEffectQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetPostProcessingQuality() != (EVideoQuality)SettingItem_PostProcessingQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetFoliageQuality() != (EVideoQuality)SettingItem_FoliageQuality->GetValue().GetIntegerValue() ||
		UVideoModule::Get().GetShadingQuality() != (EVideoQuality)SettingItem_ShadingQuality->GetValue().GetIntegerValue();
}

bool UWidgetVideoSettingPageBase::CanReset_Implementation() const
{
	return UVideoModule::Get().GetGlobalVideoQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().GlobalVideoQuality ||
		UVideoModule::Get().GetViewDistanceQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ViewDistanceQuality ||
		UVideoModule::Get().GetShadowQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ShadowQuality ||
		UVideoModule::Get().GetGlobalIlluminationQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().GlobalIlluminationQuality ||
		UVideoModule::Get().GetReflectionQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ReflectionQuality ||
		UVideoModule::Get().GetAntiAliasingQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().AntiAliasingQuality ||
		UVideoModule::Get().GetTextureQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().TextureQuality ||
		UVideoModule::Get().GetVisualEffectQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().VisualEffectQuality ||
		UVideoModule::Get().GetPostProcessingQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().PostProcessingQuality ||
		UVideoModule::Get().GetFoliageQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().FoliageQuality ||
		UVideoModule::Get().GetShadingQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().ShadingQuality;
}

FSaveData* UWidgetVideoSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().VideoData;
}
