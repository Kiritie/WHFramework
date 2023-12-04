// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class FDetailCustomizationBase : public IDetailCustomization
{
public:
	FDetailCustomizationBase();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:
	TArray< TWeakObjectPtr< UObject > > SelectedObjectsList;
};
