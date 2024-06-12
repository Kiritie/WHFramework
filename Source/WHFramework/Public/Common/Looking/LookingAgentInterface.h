// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LookingAgentInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(Blueprintable)
class ULookingAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 *
 */
class WHFRAMEWORK_API ILookingAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Looking")
	bool IsLookAtAble(AActor* InLookerActor) const;
};
