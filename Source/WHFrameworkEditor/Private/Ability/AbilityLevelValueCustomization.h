#pragma once

#include "IPropertyTypeCustomization.h"

class FAbilityLevelValueCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils) override;

private:
	FText GetPreview() const;
	TSharedPtr<IPropertyHandle> Handle;
	int32 PreviewLevel = 1;
};
