// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "StepBlueprint.generated.h"

/**
 * A Step Blueprint is essentially a specialized Blueprint whose graphs control a Step
 * The Step factory should pick this for you automatically
 */

UCLASS(BlueprintType)
class WHFRAMEWORK_API UStepBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	/** Returns the most base Step blueprint for a given blueprint (if it is inherited from another Step blueprint, returning null if only native / non-Step BP classes are it's parent) */
	static UStepBlueprint* FindRootStepBlueprint(UStepBlueprint* DerivedBlueprint);

#endif
};
