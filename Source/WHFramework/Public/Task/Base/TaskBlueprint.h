// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "TaskBlueprint.generated.h"

/**
 * A Task Blueprint is essentially a specialized Blueprint whose graphs control a Task
 * The Task factory should pick this for you automatically
 */

UCLASS(BlueprintType)
class WHFRAMEWORK_API UTaskBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	/** Returns the most base Task blueprint for a given blueprint (if it is inherited from another Task blueprint, returning null if only native / non-Task BP classes are it's parent) */
	static UTaskBlueprint* FindRootTaskBlueprint(UTaskBlueprint* DerivedBlueprint);

#endif
};
