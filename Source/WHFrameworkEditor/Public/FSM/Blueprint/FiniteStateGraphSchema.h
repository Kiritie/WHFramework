// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "FiniteStateGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UFiniteStateGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }
};
