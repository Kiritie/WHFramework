// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SaveGame/SaveGameModuleTypes.h"

#include "VideoModuleTypes.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoviePlayStartingDelegate, const FName&, InMoiveName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoviePlayFinishedDelegate, const FName&, InMoiveName);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnMoviePlayFinishedSingleDelegate, const FName&, InMoiveName);

UENUM(BlueprintType)
enum class EVideoQuality : uint8
{
	// 低
	Low UMETA(DisplayName = "低"),
	// 中
	Medium UMETA(DisplayName = "中"),
	// 高
	High UMETA(DisplayName = "高"),
	// 极高
	Epic UMETA(DisplayName = "极高"),
	// 影视级
	Cinematic UMETA(DisplayName = "影视级"),
	// 自定义
	Custom UMETA(DisplayName = "自定义", Hidden)
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVideoModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FVideoModuleSaveData()
	{
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
