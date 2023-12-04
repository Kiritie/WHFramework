// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Customization/DetailCustomizationBase.h"

#include "DetailLayoutBuilder.h"

FDetailCustomizationBase::FDetailCustomizationBase()
{
	
}

TSharedRef<IDetailCustomization> FDetailCustomizationBase::MakeInstance()
{
	return MakeShared<FDetailCustomizationBase>();
}

void FDetailCustomizationBase::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjectsList = DetailLayoutBuilder.GetSelectedObjects();
}
