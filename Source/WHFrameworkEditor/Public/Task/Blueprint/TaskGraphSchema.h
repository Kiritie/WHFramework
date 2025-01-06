// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraphSchema_K2.h"
#include "TaskGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UTaskGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	UTaskGraphSchema(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }
};
