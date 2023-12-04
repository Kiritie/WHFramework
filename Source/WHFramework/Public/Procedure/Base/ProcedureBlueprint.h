// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "ProcedureBlueprint.generated.h"

/**
 * A Procedure Blueprint is essentially a specialized Blueprint whose graphs control a procedure
 * The procedure factory should pick this for you automatically
 */

UCLASS(BlueprintType)
class WHFRAMEWORK_API UProcedureBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

#endif
};
