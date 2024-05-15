// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Customization/PropertyCustomizationBase.h"

FPropertyCustomizationBase::FPropertyCustomizationBase()
{
	
}

void FPropertyCustomizationBase::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FPropertyCustomizationBase::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

bool FPropertyCustomizationBase::ShouldInlineKey() const
{
	return IPropertyTypeCustomization::ShouldInlineKey();
}
