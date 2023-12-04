// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "AssetFactoryBase.generated.h"

class UProcedureAsset;

UCLASS(abstract, MinimalAPI)
class UAssetFactoryBase : public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	// UFactory interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface

public:
	UPROPERTY(EditAnywhere)
	TSubclassOf<UObject> ParentClass;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UObject> PickedClass;

	UPROPERTY(EditAnywhere)
	FText ClassPickerTitle;
};
