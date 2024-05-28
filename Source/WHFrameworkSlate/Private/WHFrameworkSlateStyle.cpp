// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkSlateStyle.h"

#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Application/SlateWindowHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/StyleColors.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH_SVG( RelativePath, ... ) FSlateVectorBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

TSharedPtr< ISlateStyle > FWHFrameworkSlateStyle::Instance = nullptr;

void FWHFrameworkSlateStyle::ResetToDefault()
{
	SetStyle(FWHFrameworkSlateStyle::Create());
}

void FWHFrameworkSlateStyle::SetStyle(const TSharedRef< ISlateStyle >& NewStyle)
{
	if (Instance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance.Get());
	}

	Instance = NewStyle;

	if (Instance.IsValid())
	{
		FSlateStyleRegistry::RegisterSlateStyle(*Instance.Get());
	}
	else
	{
		ResetToDefault();
	}
}

TSharedRef< ISlateStyle > FWHFrameworkSlateStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("WHFrameworkSlateStyle"));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("WHFramework"));
	if(Plugin.IsValid())
	{
		Style->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Slate")));
	}
	
	// Note, these sizes are in Slate Units.
	// Slate Units do NOT have to map to pixels.
	const FVector2D Icon5x16(5.0f, 16.0f);
	const FVector2D Icon8x4(8.0f, 4.0f);
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon10x10(10.0f, 10.0f);
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon12x16(12.0f, 16.0f);
	const FVector2D Icon14x14(14.0f, 14.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon22x22(22.0f, 22.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon25x25(25.0f, 25.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);
	const FVector2D Icon36x24(36.0f, 24.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	const FTextBlockStyle NormalTextStyle = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Regular", 9))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D::ZeroVector)
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
		.SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f/8.f)));

	const FButtonStyle DefaultButtonStyle = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.4f, 0.4f, 0.4f), 4.0f, FStyleColors::Input, 1.f))
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.2f, 0.2f, 0.2f), 4.0f, FStyleColors::Input, 1.f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.15f, 0.15f, 0.15f), 4.0f, FStyleColors::Input, 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(FStyleColors::Dropdown, 4.0f, FStyleColors::Recessed, 1.f))
		.SetNormalForeground(FStyleColors::ForegroundHover)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::Foreground);
		
	const FButtonStyle PrimaryButtonStyle = FButtonStyle()
		.SetNormal(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::Primary))
		.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::PrimaryHover))
		.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::PrimaryPress))
		.SetDisabled(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		.SetNormalForeground(FStyleColors::ForegroundHover)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::Foreground);

	const FButtonStyle& NoBorderButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>("NoBorder");

	const FTableRowStyle& NormalTableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

	// Icons
	{
		Style->Set("Icons.Box", new IMAGE_BRUSH("Common/box", FVector2D(1.f, 1.f)));
	
		Style->Set("Icons.Circle", new IMAGE_BRUSH_SVG("Common/circle", FVector2D(6.f)));
	
		Style->Set("Icons.Border_Outline_1", new BOX_BRUSH("Common/outline_border-1x1", FMargin(0.1f)));

		Style->Set("Icons.Border_Fillet_4", new BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f)));

		Style->Set("Icons.Border_Fillet_8", new BOX_BRUSH("Common/fillet-border-8x8", FMargin(8.f / 24.f)));

		Style->Set("Icons.Border_Fillet_16", new BOX_BRUSH("Common/fillet-border-16x16", FMargin(16.f / 48.f)));

		Style->Set("Icons.Arrow_Up", new IMAGE_BRUSH("Common/arrow-up", FVector2D(12.f, 7.f)));
	
		Style->Set("Icons.Arrow_Down", new IMAGE_BRUSH("Common/arrow-down", FVector2D(12.f, 7.f)));
		
		Style->Set("Icons.Close_1", new IMAGE_BRUSH("Common/close1", Icon16x16));
		
		Style->Set("Icons.Close_2", new IMAGE_BRUSH_SVG("Common/close2", Icon16x16));
		
		Style->Set("Icons.Folder", new IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f)));
	}
	
	// Buttons
	{
		Style->Set("Buttons.Default", DefaultButtonStyle);
	
		Style->Set("Buttons.Primary", PrimaryButtonStyle);
		
		Style->Set("Buttons.NoBorder", NoBorderButtonStyle);

		Style->Set("Buttons.DefaultNoBorder", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.15f, 0.15f, 0.15f)))
			.SetDisabled(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Style->Set("Buttons.DefaultNoBorder_Primary", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::PrimaryHover))
			.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::PrimaryPress))
			.SetDisabled(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Style->Set("Buttons.DefaultNoBorder_Error", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.5f, 0.2f, 0.2f)))
			.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.75f, 0.15f, 0.15f)))
			.SetDisabled(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FStyleColors::Dropdown))
		);

		Style->Set("Buttons.Tab", FButtonStyle(NoBorderButtonStyle)
			.SetNormalForeground(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			.SetHoveredForeground(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
			.SetDisabledForeground(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f)))
		);

		Style->Set("Buttons.SelectPath", FButtonStyle()
			.SetNormal(IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f), FLinearColor(0.6f, 0.6f, 0.6f)))
			.SetHovered(IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f), FLinearColor(0.9f, 0.9f, 0.9f)))
			.SetPressed(IMAGE_BRUSH_SVG("Common/folder", FVector2D(24.f, 22.f), FLinearColor(0.5f, 0.5f, 0.5f)))
		);
	}
	
	// CheckBoxes
	{
		Style->Set("CheckBoxes.DefaultNoBorder", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetUncheckedHoveredImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetUncheckedPressedImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.15f, 0.15f, 0.15f)))
			.SetCheckedImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.4f, 1.f)))
			.SetCheckedHoveredImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.4f, 1.f)))
			.SetCheckedPressedImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.4f, 1.f)))
		);

		Style->Set("CheckBoxes.Tab", FCheckBoxStyle()
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
		Style->Set("ProgressBars.Default", FProgressBarStyle()
			.SetBackgroundImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.1f, 0.1f, 0.1f)))
			.SetFillImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.f, 0.3f, 0.9f)))
			.SetMarqueeImage(FSlateNoResource())
		);
	}

	// TableRows
	{
		Style->Set("TableRows.List.Default", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);

		Style->Set("TableRows.List.Tab", FTableRowStyle(NormalTableRowStyle)
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
		
		Style->Set("TableRows.Tile.Default", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.03f, 0.03f, 0.03f)))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(4.f / 12.f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);
	}

	return Style;
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef DEFAULT_FONT
