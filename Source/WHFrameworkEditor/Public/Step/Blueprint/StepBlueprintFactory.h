// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/Blueprint.h"
#include "Factories/Factory.h"
#include "Common/Blueprint/BlueprintFactoryBase.h"
#include "StepBlueprintFactory.generated.h"

UCLASS(HideCategories=Object, MinimalAPI)
class UStepBlueprintFactory : public UBlueprintFactoryBase
{
	GENERATED_UCLASS_BODY()
};
