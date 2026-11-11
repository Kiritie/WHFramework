// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SaveGame/SaveGameModuleTypes.h"

#include "WidgetModuleTypes.generated.h"


USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWidgetModuleSaveData()
	{
		LanguageType = 0;
		GlobalScale = 1.f;
	}

public:
	UPROPERTY(meta = (DisplayName = "语言", SettingCategoryDisplayName = "全局", SettingRenderer = "Option", SettingApply = "Immediate", SettingCategory = "Global", SettingOrder = "0"))
	int32 LanguageType;

	UPROPERTY(meta = (DisplayName = "界面缩放", SettingCategoryDisplayName = "全局", ClampMin = "0.5", ClampMax = "2.0", SettingApply = "Preview", SettingCategory = "Global", SettingOrder = "10"))
	float GlobalScale;
};
