// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkSlateStyle.h"

#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Application/SlateWindowHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/StyleColors.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"

using namespace CoreStyleConstants;

TSharedPtr<ISlateStyle> FWHFrameworkSlateStyle::Instance = nullptr;

void FWHFrameworkSlateStyle::Initialize()
{
	if(!Instance.IsValid())
	{
		Instance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*Instance);
	}
}

void FWHFrameworkSlateStyle::Shutdown()
{
	if(Instance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance);
		Instance.Reset();
	}
}

const ISlateStyle& FWHFrameworkSlateStyle::Get()
{
	return *Instance;
}

FName FWHFrameworkSlateStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("WHFrameworkSlateStyle"));
	return StyleSetName;
}

TSharedRef<ISlateStyle> FWHFrameworkSlateStyle::Create()
{
	TSharedRef<class FWHFrameworkSlateStyle::FStyle> Style = MakeShareable(new FWHFrameworkSlateStyle::FStyle());
	Style->Initialize();
	return Style;
}

FWHFrameworkSlateStyle::FStyle::FStyle()
	: FSlateStyleSet(FWHFrameworkSlateStyle::GetStyleSetName())
{
}

FWHFrameworkSlateStyle::FStyle::~FStyle()
{
}

void FWHFrameworkSlateStyle::FStyle::Initialize()
{
	SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("WHFramework"))->GetBaseDir() / TEXT("Resources/Slate"));

	const FTextBlockStyle NormalTextStyle = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Regular", 9))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D::ZeroVector)
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
		.SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f/8.f)));

	const FButtonStyle DefaultButtonStyle = FButtonStyle()
		.SetNormal(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.2f)))
		.SetHovered(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.3f)))
		.SetPressed(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.1f)))
		.SetDisabled(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		.SetNormalForeground(FStyleColors::ForegroundHover)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::Foreground);
		
	const FButtonStyle PrimaryButtonStyle = FButtonStyle()
		.SetNormal(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Primary))
		.SetHovered(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::PrimaryHover))
		.SetPressed(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::PrimaryPress))
		.SetDisabled(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		.SetNormalForeground(FStyleColors::ForegroundHover)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::Foreground);
		
	const FButtonStyle ErrorButtonStyle = FButtonStyle()
		.SetNormal(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.8f, 0.f, 0.f)))
		.SetHovered(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.95f, 0.f, 0.f)))
		.SetPressed(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.5f, 0.f, 0.f)))
		.SetDisabled(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		.SetNormalForeground(FStyleColors::ForegroundHover)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::Foreground);

	const FButtonStyle& NoBorderButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>("NoBorder");

	const FTableRowStyle& NormalTableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
	
	const FEditableTextBoxStyle& NormalEditableTextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");

	// Icons
	{
		Set("Icons.Box", new IMAGE_BRUSH("Common/box", FVector2D(1.f, 1.f)));
	
		Set("Icons.Circle", new IMAGE_BRUSH_SVG("Common/circle", FVector2D(6.f)));
	
		Set("Icons.Border_Outline_1", new BOX_BRUSH("Common/outline_border-1", FMargin(1.f / 10.f)));

		Set("Icons.Border_Radius_4", new BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f)));

		Set("Icons.Border_Radius_8", new BOX_BRUSH("Common/radius-border-8", FMargin(8.f / 24.f)));

		Set("Icons.Border_Radius_16", new BOX_BRUSH("Common/radius-border-16", FMargin(16.f / 48.f)));

		Set("Icons.Border_Radius_16", new BOX_BRUSH("Common/radius-border-16", FMargin(16.f / 48.f)));

		Set("Icons.Border_Radius_16_RightTop", new BOX_BRUSH("Common/radius-border-16-right-top", FMargin(16.f / 48.f)));

		Set("Icons.Border_Radius_Outline_16", new BOX_BRUSH("Common/radius-outline-border-16", FMargin(16.f / 48.f)));

		Set("Icons.Arrow_Up", new IMAGE_BRUSH("Common/arrow-up", FVector2D(12.f, 7.f)));
	
		Set("Icons.Arrow_Down", new IMAGE_BRUSH("Common/arrow-down", FVector2D(12.f, 7.f)));
		
		Set("Icons.Close", new IMAGE_BRUSH("Common/close1", Icon16x16));
		
		Set("Icons.Close_Mini", new IMAGE_BRUSH_SVG("Common/close2", Icon16x16));
		
		Set("Icons.Folder", new IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f)));
	}
	
	// Buttons
	{
		Set("Buttons.Default", DefaultButtonStyle);
	
		Set("Buttons.Primary", PrimaryButtonStyle);
			
		Set("Buttons.Error", ErrorButtonStyle);

		Set("Buttons.NoBorder", NoBorderButtonStyle);

		Set("Buttons.DefaultNoBorder", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.2f)))
			.SetPressed(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.1f)))
			.SetDisabled(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Set("Buttons.DefaultNoBorder.Primary", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::PrimaryHover))
			.SetPressed(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::PrimaryPress))
			.SetDisabled(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Set("Buttons.DefaultNoBorder.Error", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.95f, 0.f, 0.f)))
			.SetPressed(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.5f, 0.f, 0.f)))
			.SetDisabled(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Set("Buttons.DefaultNoBorder_RightAngle", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.2f)))
			.SetPressed(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FLinearColor(1.f, 1.f, 1.f, 0.1f)))
			.SetDisabled(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Set("Buttons.DefaultNoBorder_RightAngle.Primary", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FStyleColors::PrimaryHover))
			.SetPressed(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FStyleColors::PrimaryPress))
			.SetDisabled(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Set("Buttons.DefaultNoBorder_RightAngle.Error", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FLinearColor(0.95f, 0.f, 0.f)))
			.SetPressed(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FLinearColor(0.5f, 0.f, 0.f)))
			.SetDisabled(BOX_BRUSH("Common/box", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Set("Buttons.Tab.Default", FButtonStyle(NoBorderButtonStyle)
			.SetNormalForeground(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			.SetHoveredForeground(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
			.SetPressedForeground(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f)))
		);

		Set("Buttons.Tab.Error", FButtonStyle(NoBorderButtonStyle)
			.SetNormalForeground(FSlateColor(FLinearColor(0.7f, 0.15f, 0.15f)))
			.SetHoveredForeground(FSlateColor(FLinearColor(1.f, 0.15f, 0.15f)))
			.SetPressedForeground(FSlateColor(FLinearColor(0.5f, 0.1f, 0.1f)))
		);

		Set("Buttons.Window.Close", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/radius-border-16-right-top", FMargin(16.f / 48.f), FLinearColor(0.95f, 0.f, 0.f)))
			.SetPressed(BOX_BRUSH("Common/radius-border-16-right-top", FMargin(16.f / 48.f), FLinearColor(0.5f, 0.f, 0.f)))
			.SetDisabled(BOX_BRUSH("Common/radius-border-16-right-top", FMargin(16.f / 48.f), FStyleColors::Dropdown))
		);

		Set("Buttons.SelectPath", FButtonStyle()
			.SetNormal(IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f), FLinearColor(0.6f, 0.6f, 0.6f)))
			.SetHovered(IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f), FLinearColor(0.9f, 0.9f, 0.9f)))
			.SetPressed(IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f), FLinearColor(0.5f, 0.5f, 0.5f)))
		);
	}
	
	// CheckBoxes
	{
		Set("CheckBoxes.DefaultNoBorder", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetUncheckedHoveredImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetUncheckedPressedImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.15f, 0.15f, 0.15f)))
			.SetCheckedImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.4f, 1.f)))
			.SetCheckedHoveredImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.4f, 1.f)))
			.SetCheckedPressedImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.4f, 1.f)))
		);

		Set("CheckBoxes.Tab.Default", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetUncheckedHoveredImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetUncheckedPressedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetCheckedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetCheckedHoveredImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetCheckedPressedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetUndeterminedForegroundColor(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetHoveredForegroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
			.SetCheckedForegroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);
	}

	// ProgressBars
	{
		Set("ProgressBars.Default", FProgressBarStyle()
			.SetBackgroundImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.1f, 0.1f, 0.1f)))
			.SetFillImage(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.3f, 0.9f)))
			.SetMarqueeImage(FSlateNoResource())
		);
	}

	// EditableTextBoxes
	{
		Set("EditableTextBoxes.SettingItem", FEditableTextBoxStyle(NormalEditableTextBoxStyle)
			.SetTextStyle(FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
			.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.SetBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 0.1f))
			.SetForegroundColor(FSlateColor::UseForeground())
			.SetPadding(FMargin(10.f, 8.f))
		);
	}

	// TableRows
	{
		Set("TableRows.List.Default", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);

		Set("TableRows.List.Tab", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())
			.SetInactiveBrush(FSlateNoResource())
			.SetInactiveHoveredBrush(FSlateNoResource())
			.SetTextColor(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(0.f, 0.4f, 1.f)))
		);
		
		Set("TableRows.Tile.Default", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/radius-border-4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);
	}
}
