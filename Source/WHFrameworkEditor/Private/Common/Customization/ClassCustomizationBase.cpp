// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Customization/ClassCustomizationBase.h"

#include "DetailLayoutBuilder.h"

FClassCustomizationBase::FClassCustomizationBase()
{
	
}

void FClassCustomizationBase::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();
}
