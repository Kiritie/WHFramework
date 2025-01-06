// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Asset/AssetFactoryBase.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/Blueprint.h"
#include "BlueprintFactoryBase.generated.h"

UCLASS(abstract, MinimalAPI)
class UBlueprintFactoryBase : public UAssetFactoryBase
{
	GENERATED_BODY()

public:
	UBlueprintFactoryBase(const FObjectInitializer& ObjectInitializer);

public:
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

public:
	// The type of blueprint that will be created
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EBlueprintType> BlueprintType;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UEdGraph> GraphClass;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UEdGraphSchema_K2> GraphSchemaClass;

	UPROPERTY(EditAnywhere)
	FName DefaultEventGraph;

	UPROPERTY(EditAnywhere)
	TArray<FName> DefaultEventNodes;
};
