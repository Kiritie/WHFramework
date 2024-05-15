// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class FClassCustomizationBase : public IDetailCustomization
{
public:
	FClassCustomizationBase();

public:
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:
	TArray< TWeakObjectPtr< UObject > > SelectedObjectsList;
};
