#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

#include "WorldWeatherTypes.generated.h"

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
