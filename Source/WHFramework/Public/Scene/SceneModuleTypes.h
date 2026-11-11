#pragma once
#include "Parameter/ParameterModuleTypes.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Scene/SceneAreaTypes.h"
#include "Scene/SceneMarkerTypes.h"
#include "Scene/SceneActorSaveTypes.h"
#include "Scene/WorldTimerTypes.h"
#include "Scene/WorldWeatherTypes.h"

#include "SceneModuleTypes.generated.h"

class UTexture2D;

namespace SceneTags
{
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Town);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Building);
}



UENUM(BlueprintType)
enum class EWorldTextStyle : uint8
{
	// 普通
	Normal,
	// 强调
	Stress
};

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
	TMap<FGuid, FGuid> PlayerTrackedMarkers;

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
