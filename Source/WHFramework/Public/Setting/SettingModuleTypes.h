// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Audio/AudioModuleTypes.h"
#include "Camera/CameraModuleTypes.h"
#include "Input/InputModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Video/VideoModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"

#include "SettingModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSettingModuleSaveData()
	{
		WidgetData = FWidgetModuleSaveData();
		AudioData = FAudioModuleSaveData();
		VideoData = FVideoModuleSaveData();
		CameraData = FCameraModuleSaveData();
		InputData = FInputModuleSaveData();
		ParameterData = FParameterModuleSaveData();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWidgetModuleSaveData WidgetData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAudioModuleSaveData AudioData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVideoModuleSaveData VideoData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraModuleSaveData CameraData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInputModuleSaveData InputData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FParameterModuleSaveData ParameterData;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		
		WidgetData.MakeSaved();
		AudioData.MakeSaved();
		VideoData.MakeSaved();
		CameraData.MakeSaved();
		InputData.MakeSaved();
		ParameterData.MakeSaved();
	}
};
