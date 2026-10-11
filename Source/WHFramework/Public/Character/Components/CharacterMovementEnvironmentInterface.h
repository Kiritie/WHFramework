#pragma once

#include "UObject/Interface.h"
#include "Character/Components/CharacterMovementTypes.h"
#include "CharacterMovementEnvironmentInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCharacterMovementEnvironmentInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API ICharacterMovementEnvironmentInterface
{
	GENERATED_BODY()

public:
	virtual FCharacterMovementEnvironment SampleMovementEnvironment(const FVector& InLocation) const = 0;
	virtual ECharacterWorldQueryResult QueryMovementCoverage(const FBox& InBounds) const = 0;
};
