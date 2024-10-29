// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraphSchema_K2.h"
#include "ModuleGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UModuleGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	UModuleGraphSchema(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }
};
