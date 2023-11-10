// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Audio/AudioModuleTypes.h"
#include "Camera/CameraModuleTypes.h"
#include "Input/InputModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Video/VideoModuleTypes.h"

#include "SettingModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSettingModuleSaveData()
	{
		AudioData = FAudioModuleSaveData();
		VideoData = FVideoModuleSaveData();
		CameraData = FCameraModuleSaveData();
		InputData = FInputModuleSaveData();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FAudioModuleSaveData AudioData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Video")
	FVideoModuleSaveData VideoData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FCameraModuleSaveData CameraData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FInputModuleSaveData InputData;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		
		AudioData.MakeSaved();
		VideoData.MakeSaved();
		CameraData.MakeSaved();
		InputData.MakeSaved();
	}
};
