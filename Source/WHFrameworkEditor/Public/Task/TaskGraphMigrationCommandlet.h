#pragma once

#include "Commandlets/Commandlet.h"
#include "TaskGraphMigrationCommandlet.generated.h"

UCLASS()
class WHFRAMEWORKEDITOR_API UTaskGraphMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UTaskGraphMigrationCommandlet();
	virtual int32 Main(const FString& Parameters) override;
};
