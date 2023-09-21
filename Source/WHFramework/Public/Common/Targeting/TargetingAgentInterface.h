// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetingAgentInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UTargetingAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 *
 */
class WHFRAMEWORK_API ITargetingAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Targeting")
	bool IsTargetable() const;
};
