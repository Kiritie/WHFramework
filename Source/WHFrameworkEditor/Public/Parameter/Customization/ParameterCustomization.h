// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/PropertyCustomizationBase.h"
#include "Parameter/ParameterTypes.h"

/** Customization for a primary asset id, shows an asset picker with filters */
class FParameterCustomization : public FPropertyCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FParameterCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

protected:
	void OnParameterTypeChanged();

private:
	TSharedPtr<IPropertyHandle> ParameterTypeHandle;
	
	TSharedPtr<IPropertyHandle> DescriptionHandle;
	
	TMap<EParameterType, TSharedPtr<IPropertyHandle>> ParameterValueHandles;
};
