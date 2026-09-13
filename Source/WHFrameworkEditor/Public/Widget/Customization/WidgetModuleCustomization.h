// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/ClassCustomizationBase.h"
#include "GameplayTagContainer.h"

class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class IPropertyHandleArray;
class SComboButton;
class SWidget;

class FWidgetModuleCustomization : public FClassCustomizationBase
{
public:
	FWidgetModuleCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder) override;

private:
	void BuildConfigCategory(
		IDetailLayoutBuilder& DetailLayoutBuilder,
		FName InPropertyName,
		FName InCategoryName,
		const FText& InCategoryLabel,
		bool bInWorldWidget);

	TSharedRef<SWidget> MakeTagPicker(bool bInWorldWidget);

	void OnTagPicked(FGameplayTag InWidgetTag, bool bInWorldWidget);

	FReply OnRemoveWidget(bool bInWorldWidget, uint32 InIndex);

	FReply OnValidateWidgets();

private:
	TSharedPtr<IPropertyHandleArray> UserWidgetConfigsHandle;
	TSharedPtr<IPropertyHandleArray> WorldWidgetConfigsHandle;

	TSharedPtr<SComboButton> ScreenAddButton;
	TSharedPtr<SComboButton> WorldAddButton;

	IDetailLayoutBuilder* DetailBuilder;
};
