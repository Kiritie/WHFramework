// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/PropertyCustomizationBase.h"
#include "Kismet2/EnumEditorUtils.h"

/** Customization for a primary asset id, shows an asset picker with filters */
class FEnumParameterValueCustomization : public FPropertyCustomizationBase, public FEnumEditorUtils::INotifyOnEnumChanged
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FEnumParameterValueCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	/** INotifyOnEnumChanged interface */
	virtual void PreChange(const UUserDefinedEnum* Changed, FEnumEditorUtils::EEnumEditorChangeInfo ChangedType) override;
	virtual void PostChange(const UUserDefinedEnum* Changed, FEnumEditorUtils::EEnumEditorChangeInfo ChangedType) override;

protected:
	void OnEnumTypeChanged(bool bPropertyChanged = false);

	void OnEnumValueComboChange(uint8 Index);
	TSharedRef<SWidget> OnGetEnumValueContent() const;
	FText GetCurrentEnumValueDesc() const;

private:
	TSharedPtr<IPropertyHandle> EnumTypeHandle;
	TSharedPtr<IPropertyHandle> EnumNameHandle;
	TSharedPtr<IPropertyHandle> EnumNamesHandle;
	TSharedPtr<IPropertyHandle> EnumValueHandle;

	TSharedPtr<IPropertyHandle> NotifyObserverProperty;
	
	TArray<FString> EnumPropValues;
};
