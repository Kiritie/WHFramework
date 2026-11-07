#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterModuleTypes.h"

#include "WorldTimerTypes.generated.h"

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
