// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Parameter/ParameterModuleTypes.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncUnloadLevelFinished, FName, InLevelPath);

/**
 *
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FTraceMapping
{
	GENERATED_BODY()

public:
	FTraceMapping()
	{
		TraceChannel = ECC_GameTraceChannel1;
	}

	FTraceMapping(ECollisionChannel InTraceChannel)
	{
		TraceChannel = InTraceChannel;
	}

protected:
	UPROPERTY(EditAnywhere)
	TEnumAsByte<ECollisionChannel> TraceChannel;

public:
	ECollisionChannel GetTraceChannel() const;
	
	ETraceTypeQuery GetTraceType() const;
};

/**
* 世界小地图模式
*/
UENUM(BlueprintType)
enum class EWorldMiniMapMode : uint8
{
	// 无
	None,
	// 固定位置
	FixedPoint,
	// 视角位置
	ViewPoint,
	// 相机位置
	CameraPoint
};

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
struct WHFRAMEWORK_API FWorldTimerSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWorldTimerSaveData()
	{
		DayLength = 10.f;
		NightLength = 5.f;
		TimeOfDay = 960.f;
		DateTime = -1.f;
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
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWeatherSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWorldWeatherSaveData()
	{
		WeatherSeed = 0;
		WeatherParams = TArray<FParameter>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WeatherSeed;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FParameter> WeatherParams;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneActorSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSceneActorSaveData()
	{
		ActorID = FGuid();
		SpawnTransform = FTransform::Identity;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FGuid ActorID;

	UPROPERTY()
	FTransform SpawnTransform;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSceneModuleSaveData()
	{
		MiniMapRange = 512.f;
		TimerData = FWorldTimerSaveData();
		WeatherData = FWorldWeatherSaveData();
		ActorSaveDatas = TArray<FSceneActorSaveData>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MiniMapRange;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWorldTimerSaveData TimerData;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWorldWeatherSaveData WeatherData;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSceneActorSaveData> ActorSaveDatas;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		TimerData.MakeSaved();
		WeatherData.MakeSaved();

		for(auto& Iter : ActorSaveDatas)
		{
			Iter.MakeSaved();
		}
	}
};
