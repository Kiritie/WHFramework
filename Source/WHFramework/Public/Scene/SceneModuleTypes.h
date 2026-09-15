#pragma once
#include "Parameter/ParameterModuleTypes.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "SceneModuleTypes.generated.h"

class UTexture2D;

namespace SceneTags
{
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Town);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Building);
}

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSceneMarkersChanged);

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

UENUM(BlueprintType)
enum class EWorldMiniMapSource : uint8
{
	None,
	SceneCapture,
	Widget
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneMapView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Range = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Yaw = 0.f;
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
enum class ESceneAreaShape : uint8
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
enum class ESceneAreaType : uint8
{
	Default,
	Chunk
};

UENUM(BlueprintType, meta = (Bitflags))
enum class ESceneMarkerChannel : uint8
{
	None = 0,
	Map = 1 << 0,
	MiniMap = 1 << 1,
	Compass = 1 << 2,
	World = 1 << 3
};
ENUM_CLASS_FLAGS(ESceneMarkerChannel);

/**
 *
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneArea
{
	GENERATED_BODY()

public:
	FSceneArea()
	{
		AreaName = NAME_None;
		AreaDisplayName = FText::GetEmpty();
		AreaType = ESceneAreaType::Default;
		AreaShape = ESceneAreaShape::Box;
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
	ESceneAreaType AreaType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESceneAreaShape AreaShape;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == ESceneAreaShape::Box || AreaShape == ESceneAreaShape::Ellipse"))
	FVector2D AreaCenter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == ESceneAreaShape::Box || AreaShape == ESceneAreaShape::Ellipse"))
	FVector2D AreaRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditConditionHides, EditCondition = "AreaShape == ESceneAreaShape::Polygon"))
	TArray<FVector2D> AreaPoints;

	/** 生成结构的首选世界空间导航点 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector EntranceLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer FeatureTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPrimaryAssetId SourceAssetID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGuid EncounterID;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSceneAreaAdded, const FSceneArea&);
DECLARE_MULTICAST_DELEGATE(FOnSceneAreasChanged);
DECLARE_DELEGATE_RetVal_TwoParams(FSceneArea, FSceneAreaResolver, const FSceneArea&, const FVector2D&);

/** 世界地图、小地图、罗盘与世界空间界面共用的标记 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid MarkerID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag MarkerTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::White;

	/** 世界空间备用位置，绑定角色卸载时更新 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid ActorID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "/Script/WHFramework.ESceneMarkerChannel"))
	int32 Channels = static_cast<int32>(ESceneMarkerChannel::Map);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinDistance = 0.f;

	/** 零表示不限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bPersistent = false;

	bool HasChannel(ESceneMarkerChannel InChannel) const
	{
		return EnumHasAnyFlags(static_cast<ESceneMarkerChannel>(Channels), InChannel);
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneMarkerView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FSceneMarker Marker;

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float Distance = 0.f;

	/** 相对给定视角偏航的有符号角度 */
	UPROPERTY(BlueprintReadOnly)
	float Bearing = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bTargetLoaded = false;

	UPROPERTY(BlueprintReadOnly)
	bool bTracked = false;

	UPROPERTY(BlueprintReadOnly)
	bool bPlayer = false;
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

USTRUCT()
struct FSoftLevelPathArrayParameterValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSoftLevelPath> Value;
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
	UPROPERTY(BlueprintReadWrite, meta = (IgnoreForMemberInitializationTest))
	FGuid ActorID;

	UPROPERTY()
	FTransform SpawnTransform;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneActorSaveRecord
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGuid ActorId;

	UPROPERTY()
	FPrimaryAssetId AssetId;

	UPROPERTY()
	FSoftClassPath ActorClass;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	bool bRuntimeSpawned = false;

	UPROPERTY()
	bool bDestroyed = false;

	UPROPERTY()
	FParameter Data;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSceneModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSceneModuleSaveData()
	{
		MiniMapRange = 512.f;
		WorldMapCenter = FVector2D::ZeroVector;
		WorldMapRange = 4096.f;
		SceneAreas = TArray<FSceneArea>();
		TimerData = FWorldTimerSaveData();
		WeatherData = FWorldWeatherSaveData();
		ActorSaveRecords = TArray<FSceneActorSaveRecord>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MiniMapRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D WorldMapCenter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WorldMapRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid TrackedMarkerID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSceneMarker> Markers;
				
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSceneArea> SceneAreas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWorldTimerSaveData TimerData;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWorldWeatherSaveData WeatherData;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSceneActorSaveRecord> ActorSaveRecords;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		TimerData.MakeSaved();
		WeatherData.MakeSaved();

		for(FSceneActorSaveRecord& Iter : ActorSaveRecords)
		{
			const UScriptStruct* StructType = Iter.Data.GetStructType();
			if(StructType && StructType->IsChildOf(FSaveData::StaticStruct()))
			{
				reinterpret_cast<FSaveData*>(Iter.Data.GetMutableStructMemory())->MakeSaved();
			}
		}
	}
};
