#pragma once

#include "Factories/Factory.h"
#include "WidgetThemeFactory.generated.h"

UCLASS()
class UWidgetThemeFactory : public UFactory
{
	GENERATED_BODY()

public:
	UWidgetThemeFactory();
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
