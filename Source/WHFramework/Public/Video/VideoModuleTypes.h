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
		WindowResolution = FIntPoint::ZeroValue;
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
		AmbientOcclusionQuality = EVideoQuality::Epic;
	}

public:
	UPROPERTY(meta = (DisplayName = "窗口模式", SettingCategoryDisplayName = "显示", SettingCategory = "Display", SettingOrder = "0", SettingRequiresConfirmation))
	EWindowModeN WindowMode;

	UPROPERTY(meta = (DisplayName = "分辨率", SettingCategoryDisplayName = "显示", SettingLeaf, SettingRenderer = "Option", SettingApply = "Deferred", SettingCategory = "Display", SettingOrder = "10", SettingRequiresConfirmation))
	FIntPoint WindowResolution;

	UPROPERTY(meta = (DisplayName = "垂直同步", SettingCategoryDisplayName = "显示", SettingCategory = "Display", SettingOrder = "20"))
	bool bEnableVSync;

	UPROPERTY(meta = (DisplayName = "动态分辨率", SettingCategoryDisplayName = "显示", SettingCategory = "Display", SettingOrder = "30"))
	bool bEnableDynamicResolution;

	UPROPERTY(meta = (DisplayName = "整体画质", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "40"))
	EVideoQuality GlobalVideoQuality;

	UPROPERTY(meta = (DisplayName = "视距", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "50"))
	EVideoQuality ViewDistanceQuality;

	UPROPERTY(meta = (DisplayName = "阴影", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "60"))
	EVideoQuality ShadowQuality;

	UPROPERTY(meta = (DisplayName = "全局光照", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "70"))
	EVideoQuality GlobalIlluminationQuality;

	UPROPERTY(meta = (DisplayName = "反射", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "80"))
	EVideoQuality ReflectionQuality;

	UPROPERTY(meta = (DisplayName = "抗锯齿", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "90"))
	EVideoQuality AntiAliasingQuality;

	UPROPERTY(meta = (DisplayName = "纹理", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "100"))
	EVideoQuality TextureQuality;

	UPROPERTY(meta = (DisplayName = "特效", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "110"))
	EVideoQuality VisualEffectQuality;

	UPROPERTY(meta = (DisplayName = "后期处理", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "120"))
	EVideoQuality PostProcessingQuality;

	UPROPERTY(meta = (DisplayName = "植被", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "130"))
	EVideoQuality FoliageQuality;

	UPROPERTY(meta = (DisplayName = "着色", SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "140"))
	EVideoQuality ShadingQuality;

	UPROPERTY(meta = (SettingCategoryDisplayName = "画质", SettingCategory = "Quality", SettingOrder = "150", DisplayName = "环境光遮蔽（SSAO）"))
	EVideoQuality AmbientOcclusionQuality;
};
