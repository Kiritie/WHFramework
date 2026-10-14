#pragma once

#include "Commandlets/Commandlet.h"
#include "WidgetThemeValidationCommandlet.generated.h"

UCLASS()
class UWidgetThemeValidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWidgetThemeValidationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
