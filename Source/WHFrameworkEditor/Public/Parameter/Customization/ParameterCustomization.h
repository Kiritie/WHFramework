// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/PropertyCustomizationBase.h"

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
	
	TSharedPtr<IPropertyHandle> IntegerValueHandle;

	TSharedPtr<IPropertyHandle> FloatValueHandle;

	TSharedPtr<IPropertyHandle> ByteValueHandle;

	TSharedPtr<IPropertyHandle> EnumValueHandle;

	TSharedPtr<IPropertyHandle> StringValueHandle;

	TSharedPtr<IPropertyHandle> NameValueHandle;

	TSharedPtr<IPropertyHandle> TextValueHandle;

	TSharedPtr<IPropertyHandle> BooleanValueHandle;

	TSharedPtr<IPropertyHandle> VectorValueHandle;

	TSharedPtr<IPropertyHandle> RotatorValueHandle;

	TSharedPtr<IPropertyHandle> ColorValueHandle;

	TSharedPtr<IPropertyHandle> KeyValueHandle;

	TSharedPtr<IPropertyHandle> TagValueHandle;

	TSharedPtr<IPropertyHandle> TagsValueHandle;

	TSharedPtr<IPropertyHandle> AssetIDValueHandle;

	TSharedPtr<IPropertyHandle> ClassValueHandle;

	TSharedPtr<IPropertyHandle> ClassPtrValueHandle;

	TSharedPtr<IPropertyHandle> ObjectValueHandle;

	TSharedPtr<IPropertyHandle> ObjectPtrValueHandle;

	TSharedPtr<IPropertyHandle> DelegateValueHandle;
};
