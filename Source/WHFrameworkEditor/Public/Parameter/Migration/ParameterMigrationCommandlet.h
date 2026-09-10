#pragma once

#include "Commandlets/Commandlet.h"
#include "ParameterMigrationCommandlet.generated.h"

/** Records every serialized FParameter before the V2 layout migration. */
UCLASS()
class UParameterMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UParameterMigrationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
