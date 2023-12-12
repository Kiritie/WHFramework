// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Parameter/ParameterModuleTypes.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncUnloadLevelFinished, FName, InLevelPath);

/**
* 世界文本风格
*/
UENUM(BlueprintType)
enum class EWorldTextStyle : uint8
{
	// 普通
	Normal,
	// 强调
	Stress
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSceneModuleSaveData()
	{
		DayLength = 10.f;
		NightLength = 5.f;
		TimeOfDay = 960.f;
		DateTime = -1.f;

		WeatherSeed = 0;
		WeatherParams = TArray<FParameter>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DayLength;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NightLength;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeOfDay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FDateTime DateTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WeatherSeed;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FParameter> WeatherParams;

public:
	virtual bool IsValid() const override
	{
		return !Datas.IsEmpty();
	}
	
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
	}
};
