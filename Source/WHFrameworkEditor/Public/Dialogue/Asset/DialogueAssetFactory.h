#pragma once

#include "Factories/Factory.h"
#include "DialogueAssetFactory.generated.h"

/**
 *
 */
UCLASS()
class WHFRAMEWORKEDITOR_API UDialogueAssetFactory : public UFactory
{
	GENERATED_BODY()

	UDialogueAssetFactory(const FObjectInitializer& ObjectInitializer);

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface

	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual FString GetDefaultNewAssetName() const override;
};
