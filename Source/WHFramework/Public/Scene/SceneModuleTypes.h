// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Parameter/ParameterModuleTypes.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);

/**
 * 时间阶段
 */
UENUM(BlueprintType)
enum class EWorldTimePhase : uint8
{
	None,
	//早晨
	Morning,
	//上午
	Forenoon,
	//中午
	Noon,
	//下午
	Afternoon,
	//傍晚
	Evening,
	//晚上
	Night
};

/**
 * 加载关卡状态
 */
UENUM(BlueprintType)
enum class EFAsyncLoadLevelState : uint8
{
	None,
	//加载
	Loading,
	//卸载
	Unloading
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

/**
* 世界大地图区域形状
*/
UENUM(BlueprintType)
enum class EWorldMaxMapAreaShape : uint8
{
	// 方形
	Box,
	// 椭圆
	Ellipse,
	// 多边形
	Polygon
};

/**
* 世界大地图区域类型
*/
UENUM(BlueprintType)
enum class EWorldMaxMapAreaType : uint8
{
	// 区域1
	Area1,
	// 区域2
	Area2,
	// 区域3
	Area3,
	// 区域4
	Area4,
	// 区域5
	Area5,
	// 区域6
	Area6,
	// 区域7
	Area7,
	// 区域8
	Area8,
	// 区域9
	Area9,
	// 区域10
	Area10
};

/**
 *
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldMaxMapArea
{
	GENERATED_BODY()

public:
	FWorldMaxMapArea()
	{
		AreaName = NAME_None;
		AreaDisplayName = FText::GetEmpty();
		AreaType = EWorldMaxMapAreaType::Area1;
		AreaShape = EWorldMaxMapAreaShape::Box;
		AreaCenter = FVector2D::ZeroVector;
		AreaRadius = FVector2D::ZeroVector;
		AreaPoints = TArray<FVector2D>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText AreaDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWorldMaxMapAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EWorldMaxMapAreaShape AreaShape;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == EWorldMaxMapAreaShape::Box || AreaShape == EWorldMaxMapAreaShape::Ellipse"))
	FVector2D AreaCenter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == EWorldMaxMapAreaShape::Box || AreaShape == EWorldMaxMapAreaShape::Ellipse"))
	FVector2D AreaRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == EWorldMaxMapAreaShape::Polygon"))
	TArray<FVector2D> AreaPoints;
};

/**
 *
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSoftLevelPath
{
	GENERATED_BODY()

public:
	FSoftLevelPath()
	{
		LevelObjectPtr = nullptr;
		LevelPath = NAME_None;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UWorld> LevelObjectPtr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditConditionHides, EditCondition = "!LevelObjectPtr.IsValid()"))
	FName LevelPath;
};

/**
 *
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAsyncLoadLevelTask
{
	GENERATED_BODY()

public:
	FAsyncLoadLevelTask()
	{
		bLoading = false;
		State = EFAsyncLoadLevelState::None;
		LevelPath = NAME_None;
		OnLoadFinished = FOnAsyncLoadLevelFinished();
		FinishDelayTime = 0.f;
		bCreateLoadingWidget = false;
	}

public:
	UPROPERTY(VisibleAnywhere)
	bool bLoading;

	UPROPERTY(VisibleAnywhere)
	EFAsyncLoadLevelState State;

	UPROPERTY(VisibleAnywhere)
	FName LevelPath;

	UPROPERTY(VisibleAnywhere)
	FOnAsyncLoadLevelFinished OnLoadFinished;

	UPROPERTY(VisibleAnywhere)
	float FinishDelayTime;

	UPROPERTY(VisibleAnywhere)
	bool bCreateLoadingWidget;
};

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
		DateTime = FDateTime();
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
		ActorID = FGuid::NewGuid();
		SpawnTransform = FTransform::Identity;
	}

public:
	UPROPERTY(BlueprintReadWrite)
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
		MaxMapAreas = TArray<FWorldMaxMapArea>();
		TimerData = FWorldTimerSaveData();
		WeatherData = FWorldWeatherSaveData();
		ActorSaveDatas = TArray<FSceneActorSaveData>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MiniMapRange;
				
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FWorldMaxMapArea> MaxMapAreas;

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
