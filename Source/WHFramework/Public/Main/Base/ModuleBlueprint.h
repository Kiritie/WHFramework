// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "ModuleBlueprint.generated.h"

/**
 * A Module Blueprint is essentially a specialized Blueprint whose graphs control a procedure
 * The procedure factory should pick this for you automatically
 */

UCLASS(BlueprintType)
class WHFRAMEWORK_API UModuleBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:
	UModuleBlueprint(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	/** Returns the most base procedure blueprint for a given blueprint (if it is inherited from another procedure blueprint, returning null if only native / non-procedure BP classes are it's parent) */
	static UModuleBlueprint* FindModuleBlueprint(UModuleBlueprint* DerivedBlueprint);

#endif
};
