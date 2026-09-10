#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VitalitySpawned.generated.h"

class IAbilityVitalityInterface;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVitalitySpawned : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVitalitySpawned() = default;

	FEventVitalitySpawned(TScriptInterface<IAbilityVitalityInterface> InVitality, TScriptInterface<IAbilityVitalityInterface> InRescuer)
		: Vitality(MoveTemp(InVitality)), Rescuer(MoveTemp(InRescuer))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IAbilityVitalityInterface> Vitality = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IAbilityVitalityInterface> Rescuer = nullptr;
};
