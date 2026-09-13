#pragma once

#include "Common/Customization/ClassCustomizationBase.h"

class IDetailLayoutBuilder;

class FSettingRegistryCustomization : public FClassCustomizationBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder) override;

private:
	FReply OnGenerateSnapshot();

	IDetailLayoutBuilder* DetailBuilder = nullptr;
};
