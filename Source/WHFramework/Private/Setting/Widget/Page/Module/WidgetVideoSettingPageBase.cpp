// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetVideoSettingPageBase.h"

#include "Common/CommonModuleStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"
#include "Video/VideoModule.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetVideoSettingPageBase::UWidgetVideoSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Title = NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Video", "视频");
}

void UWidgetVideoSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);

	SettingItem_WindowMode = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "WindowMode", "窗口模式"), FString("/Script/WHFramework.EWindowModeN") }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_WindowMode->SetValue((int32)UVideoModule::Get().GetWindowMode());
	AddSettingItem(FName("WindowMode"), SettingItem_WindowMode, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "General", "通用"));

	TArray<int32> TmpArr1;
	for(int32 i = 1; i < UCommonModuleStatics::GetEnumItemNum(FString("/Script/WHFramework.EWindowResolution")); i++)
	{
		FString StrX, StrY;
		UCommonModuleStatics::GetEnumDisplayNameByValue(FString("/Script/WHFramework.EWindowResolution"), i).ToString().Split(TEXT("x"), &StrX, &StrY);
		if(FCString::Atoi(*StrX) > UVideoModule::Get().GetDesktopResolution().X || FCString::Atoi(*StrY) > UVideoModule::Get().GetDesktopResolution().Y)
		{
			TmpArr1.Add(i);
		}
	}
	SettingItem_WindowResolution = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Resolution", "分辨率"), FString("/Script/WHFramework.EWindowResolution"), FInt32ArrayParameterValue(TmpArr1) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_WindowResolution->SetValue((int32)UVideoModule::Get().GetWindowResolution());
	AddSettingItem(FName("WindowResolution"), SettingItem_WindowResolution, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "General", "通用"));
	
	SettingItem_EnableVSync = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "VerticalSync", "垂直同步") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_EnableVSync->SetValue(UVideoModule::Get().IsEnableVSync());
	AddSettingItem(FName("EnableVSync"), SettingItem_EnableVSync, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "General", "通用"));
	
	SettingItem_EnableDynamicResolution = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "DynamicResolution", "动态分辨率") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_EnableDynamicResolution->SetValue(UVideoModule::Get().IsEnableDynamicResolution());
	AddSettingItem(FName("EnableDynamicResolution"), SettingItem_EnableDynamicResolution, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "General", "通用"));

	SettingItem_GlobalVideoQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Preset", "预设"), FString("/Script/WHFramework.EVideoQuality") }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_GlobalVideoQuality->SetValue((int32)UVideoModule::Get().GetGlobalVideoQuality());
	AddSettingItem(FName("GlobalVideoQuality"), SettingItem_GlobalVideoQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	TArray<int32> TmpArr2 = { 5 };
	SettingItem_ViewDistanceQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "ViewDistance", "渲染距离"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_ViewDistanceQuality->SetValue((int32)UVideoModule::Get().GetViewDistanceQuality());
	AddSettingItem(FName("ViewDistanceQuality"), SettingItem_ViewDistanceQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_ShadowQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Shadows", "阴影"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_ShadowQuality->SetValue((int32)UVideoModule::Get().GetShadowQuality());
	AddSettingItem(FName("ShadowQuality"), SettingItem_ShadowQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_GlobalIlluminationQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GlobalIllumination", "光照"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_GlobalIlluminationQuality->SetValue((int32)UVideoModule::Get().GetGlobalIlluminationQuality());
	AddSettingItem(FName("GlobalIlluminationQuality"), SettingItem_GlobalIlluminationQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_ReflectionQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Reflections", "反射"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_ReflectionQuality->SetValue((int32)UVideoModule::Get().GetReflectionQuality());
	AddSettingItem(FName("ReflectionQuality"), SettingItem_ReflectionQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_AntiAliasingQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "AntiAliasing", "抗锯齿"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_AntiAliasingQuality->SetValue((int32)UVideoModule::Get().GetAntiAliasingQuality());
	AddSettingItem(FName("AntiAliasingQuality"), SettingItem_AntiAliasingQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_TextureQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Textures", "贴图"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_TextureQuality->SetValue((int32)UVideoModule::Get().GetTextureQuality());
	AddSettingItem(FName("TextureQuality"), SettingItem_TextureQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_VisualEffectQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Effects", "视觉效果"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_VisualEffectQuality->SetValue((int32)UVideoModule::Get().GetVisualEffectQuality());
	AddSettingItem(FName("VisualEffectQuality"), SettingItem_VisualEffectQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_PostProcessingQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "PostProcessing", "后处理"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_PostProcessingQuality->SetValue((int32)UVideoModule::Get().GetPostProcessingQuality());
	AddSettingItem(FName("PostProcessingQuality"), SettingItem_PostProcessingQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_FoliageQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "Foliage", "植被"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_FoliageQuality->SetValue((int32)UVideoModule::Get().GetFoliageQuality());
	AddSettingItem(FName("FoliageQuality"), SettingItem_FoliageQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));

	SettingItem_ShadingQuality = UObjectPoolModuleStatics::SpawnObject<UWidgetEnumSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetVideoSettingPageBase", "AmbientOcclusion", "环境光遮蔽"), FString("/Script/WHFramework.EVideoQuality"), FInt32ArrayParameterValue(TmpArr2) }, USettingModule::Get().GetEnumSettingItemClass());
	SettingItem_ShadingQuality->SetValue((int32)UVideoModule::Get().GetShadingQuality());
	AddSettingItem(FName("ShadingQuality"), SettingItem_ShadingQuality, NSLOCTEXT("WH.WidgetVideoSettingPageBase", "GraphicsQuality", "画质"));
}

void UWidgetVideoSettingPageBase::OnApply()
{
	Super::OnApply();

	UVideoModule::Get().SetWindowMode((EWindowModeN)SettingItem_WindowMode->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetWindowResolution((EWindowResolution)SettingItem_WindowResolution->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetEnableVSync(SettingItem_EnableVSync->GetValue().Get<bool>());
	UVideoModule::Get().SetEnableDynamicResolution(SettingItem_EnableDynamicResolution->GetValue().Get<bool>());
	UVideoModule::Get().SetGlobalVideoQuality((EVideoQuality)SettingItem_GlobalVideoQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetViewDistanceQuality((EVideoQuality)SettingItem_ViewDistanceQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetShadowQuality((EVideoQuality)SettingItem_ShadowQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetGlobalIlluminationQuality((EVideoQuality)SettingItem_GlobalIlluminationQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetReflectionQuality((EVideoQuality)SettingItem_ReflectionQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetAntiAliasingQuality((EVideoQuality)SettingItem_AntiAliasingQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetTextureQuality((EVideoQuality)SettingItem_TextureQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetVisualEffectQuality((EVideoQuality)SettingItem_VisualEffectQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetPostProcessingQuality((EVideoQuality)SettingItem_PostProcessingQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetFoliageQuality((EVideoQuality)SettingItem_FoliageQuality->GetValue().Get<int32>(), false);
	UVideoModule::Get().SetShadingQuality((EVideoQuality)SettingItem_ShadingQuality->GetValue().Get<int32>(), false);

	UVideoModule::Get().ApplyVideoSettings();
}

void UWidgetVideoSettingPageBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UWidgetVideoSettingPageBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	SettingItem_WindowMode->SetValue((int32)UVideoModule::Get().GetWindowMode());
	SettingItem_WindowResolution->SetValue((int32)UVideoModule::Get().GetWindowResolution());
	SettingItem_EnableVSync->SetValue(UVideoModule::Get().IsEnableVSync());
	SettingItem_EnableDynamicResolution->SetValue(UVideoModule::Get().IsEnableDynamicResolution());
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

void UWidgetVideoSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

	SettingItem_WindowMode->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().WindowMode);
	SettingItem_WindowResolution->SetValue((int32)GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().WindowResolution);
	SettingItem_EnableVSync->SetValue(GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().bEnableVSync);
	SettingItem_EnableDynamicResolution->SetValue(GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().bEnableDynamicResolution);
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
		if(InValue.Get<int32>() != (int32)EVideoQuality::Custom)
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
	else if(InSettingItem->GetNameS().ToString().EndsWith(TEXT("Quality")))
	{
		if(InValue != SettingItem_GlobalVideoQuality->GetValue())
		{
			SettingItem_GlobalVideoQuality->SetValue((int32)EVideoQuality::Custom);
		}
	}
}

bool UWidgetVideoSettingPageBase::CanApply_Implementation() const
{
	return UVideoModule::Get().GetWindowMode() != (EWindowModeN)SettingItem_WindowMode->GetValue().Get<int32>() ||
		UVideoModule::Get().GetWindowResolution() != (EWindowResolution)SettingItem_WindowResolution->GetValue().Get<int32>() ||
		UVideoModule::Get().IsEnableVSync() != SettingItem_EnableVSync->GetValue().Get<bool>() ||
		UVideoModule::Get().IsEnableDynamicResolution() != SettingItem_EnableDynamicResolution->GetValue().Get<bool>() ||
		UVideoModule::Get().GetGlobalVideoQuality() != (EVideoQuality)SettingItem_GlobalVideoQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetViewDistanceQuality() != (EVideoQuality)SettingItem_ViewDistanceQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetShadowQuality() != (EVideoQuality)SettingItem_ShadowQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetGlobalIlluminationQuality() != (EVideoQuality)SettingItem_GlobalIlluminationQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetReflectionQuality() != (EVideoQuality)SettingItem_ReflectionQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetAntiAliasingQuality() != (EVideoQuality)SettingItem_AntiAliasingQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetTextureQuality() != (EVideoQuality)SettingItem_TextureQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetVisualEffectQuality() != (EVideoQuality)SettingItem_VisualEffectQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetPostProcessingQuality() != (EVideoQuality)SettingItem_PostProcessingQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetFoliageQuality() != (EVideoQuality)SettingItem_FoliageQuality->GetValue().Get<int32>() ||
		UVideoModule::Get().GetShadingQuality() != (EVideoQuality)SettingItem_ShadingQuality->GetValue().Get<int32>();
}

bool UWidgetVideoSettingPageBase::CanReset_Implementation() const
{
	return UVideoModule::Get().GetWindowMode() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().WindowMode ||
		UVideoModule::Get().GetWindowResolution() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().WindowResolution ||
		UVideoModule::Get().IsEnableVSync() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().bEnableVSync ||
		UVideoModule::Get().IsEnableDynamicResolution() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().bEnableDynamicResolution ||
		UVideoModule::Get().GetGlobalVideoQuality() != GetDefaultSaveData()->CastRef<FVideoModuleSaveData>().GlobalVideoQuality ||
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
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().VideoData;
}
