// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SaveGame/SaveGameModuleTypes.h"

#include "VideoModuleTypes.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoviePlayStartingDelegate, const FName&, InMoiveName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoviePlayFinishedDelegate, const FName&, InMoiveName);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnMoviePlayFinishedSingleDelegate, const FName&, InMoiveName);

UENUM(BlueprintType)
enum class EWindowModeN : uint8
{
	// 全屏
	Fullscreen,
	// 无边框全屏
	WindowedFullscreen,
	// 窗口化
	Windowed
};

UENUM(BlueprintType)
enum class EWindowResolution : uint8
{
	WR_ScreenSize UMETA(DisplayName = "ScreenSize"),
	WR_3840_2160 UMETA(DisplayName = "3840x2160"),
	WR_2560_1440 UMETA(DisplayName = "2560x1440"),
	WR_1920_1080 UMETA(DisplayName = "1920x1080"),
	WR_1600_900 UMETA(DisplayName = "1600x900"),
	WR_1366_768 UMETA(DisplayName = "1366x768"),
	WR_1280_720 UMETA(DisplayName = "1280x720"),
	WR_1024_576 UMETA(DisplayName = "1024x576"),
	WR_960_540 UMETA(DisplayName = "960x540")
};

UENUM(BlueprintType)
enum class EVideoQuality : uint8
{
	// 低
	Low,
	// 中
	Medium,
	// 高
	High,
	// 极高
	Epic,
	// 影视级
	Cinematic,
	// 自定义
	Custom UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVideoModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVideoModuleSaveData()
	{
		WindowMode = EWindowModeN::Fullscreen;
		WindowResolution = EWindowResolution::WR_ScreenSize;
		bEnableVSync = false;
		bEnableDynamicResolution = false;
		GlobalVideoQuality = EVideoQuality::Epic;
		ViewDistanceQuality = EVideoQuality::Epic;
		ShadowQuality = EVideoQuality::Epic;
		GlobalIlluminationQuality = EVideoQuality::Epic;
		ReflectionQuality = EVideoQuality::Epic;
		AntiAliasingQuality = EVideoQuality::Epic;
		TextureQuality = EVideoQuality::Epic;
		VisualEffectQuality = EVideoQuality::Epic;
		PostProcessingQuality = EVideoQuality::Epic;
		FoliageQuality = EVideoQuality::Epic;
		ShadingQuality = EVideoQuality::Epic;
	}

public:
	UPROPERTY()
	EWindowModeN WindowMode;

	UPROPERTY()
	EWindowResolution WindowResolution;

	UPROPERTY()
	bool bEnableVSync;

	UPROPERTY()
	bool bEnableDynamicResolution;

	UPROPERTY()
	EVideoQuality GlobalVideoQuality;

	UPROPERTY()
	EVideoQuality ViewDistanceQuality;

	UPROPERTY()
	EVideoQuality ShadowQuality;

	UPROPERTY()
	EVideoQuality GlobalIlluminationQuality;

	UPROPERTY()
	EVideoQuality ReflectionQuality;

	UPROPERTY()
	EVideoQuality AntiAliasingQuality;

	UPROPERTY()
	EVideoQuality TextureQuality;

	UPROPERTY()
	EVideoQuality VisualEffectQuality;

	UPROPERTY()
	EVideoQuality PostProcessingQuality;

	UPROPERTY()
	EVideoQuality FoliageQuality;

	UPROPERTY()
	EVideoQuality ShadingQuality;
};
