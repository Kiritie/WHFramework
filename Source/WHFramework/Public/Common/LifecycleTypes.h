#pragma once

#include "CoreMinimal.h"

#include "LifecycleTypes.generated.h"

UENUM(BlueprintType)
enum class EPauseMode : uint8
{
	Default,
	TimeOnly,
	ModuleOnly,
	TimeAndModule
};

UENUM(BlueprintType)
enum class EPhase : uint8
{
	None = 0,
	Primary = 1 << 0,
	Lesser = 1 << 1,
	Final = 1 << 2,
	PrimaryAndLesser = Primary | Lesser,
	LesserAndFinal = Lesser | Final,
	PrimaryAndFinal = Primary | Final,
	All = Primary | Lesser | Final
};

/**
 * ???????????
 */

#define ITER_PHASE(Phase, Expression) \
EPhase Phase = EPhase::Primary; \
Expression \
Phase = EPhase::Lesser; \
Expression \
Phase = EPhase::Final; \
Expression
