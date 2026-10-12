#pragma once

#include "Commandlets/Commandlet.h"
#include "StepImportCommandlet.generated.h"

UCLASS()
class WHFRAMEWORKEDITOR_API UStepImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UStepImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
