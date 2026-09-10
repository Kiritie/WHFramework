#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_VitalityDead.generated.h"

class IAbilityVitalityInterface;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventVitalityDead : public FEventBase
{
	GENERATED_BODY()

public:
	FEventVitalityDead() = default;

	FEventVitalityDead(TScriptInterface<IAbilityVitalityInterface> InVitality, TScriptInterface<IAbilityVitalityInterface> InKiller)
		: Vitality(MoveTemp(InVitality)), Killer(MoveTemp(InKiller))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IAbilityVitalityInterface> Vitality = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IAbilityVitalityInterface> Killer = nullptr;
};
