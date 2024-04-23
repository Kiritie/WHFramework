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

	TArray<FString> PluginPaths;
	IPluginManager::Get().FindPluginsUnderDirectory(FPaths::ProjectPluginsDir(), PluginPaths);
	for(auto Iter : PluginPaths)
	{
		FString Path, Filename, Extension;
		FPaths::Split(Iter, Path, Filename, Extension);
		if(Filename == TEXT("WHFramework"))
		{
			Style->SetContentRoot(Path / TEXT("Content/Slate"));
			break;
		}
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
		.SetNormal(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FStyleColors::Primary))
		.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FStyleColors::PrimaryHover))
		.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FStyleColors::PrimaryPress))
		.SetDisabled(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FStyleColors::Dropdown))
		.SetNormalForeground(FStyleColors::ForegroundHover)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::Foreground);

	const FButtonStyle& NoBorderButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>("NoBorder");

	const FTableRowStyle& NormalTableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

	// Icons
	{
		Style->Set("Icons.White", new IMAGE_BRUSH("Common/white-1x1", FVector2D(1.f, 1.f)));
	
		Style->Set("Icons.Border_Outline_1", new BOX_BRUSH("Common/border-10x10", FMargin(0.1f)));

		Style->Set("Icons.Fillet_Border_4", new BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f)));

		Style->Set("Icons.Fillet_Border_16", new BOX_BRUSH("Common/fillet-border-16x16", FMargin(0.4f)));

		Style->Set("Icons.Arrow_Up", new IMAGE_BRUSH("Common/arrow-up", FVector2D(12.f, 7.f)));
	
		Style->Set("Icons.Arrow_Down", new IMAGE_BRUSH("Common/arrow-down", FVector2D(12.f, 7.f)));

		Style->Set("Icons.Download", new IMAGE_BRUSH_SVG("Common/download", Icon16x16));
		
		Style->Set("Icons.Close", new IMAGE_BRUSH("Common/close", Icon16x16));

		Style->Set("Icons.Clear", new IMAGE_BRUSH("Common/close", Icon16x16));

		Style->Set("Icons.Setting", new IMAGE_BRUSH("Common/setting", Icon32x32));

		Style->Set("Icons.Package", new IMAGE_BRUSH_SVG("Common/package", Icon32x32));

		Style->Set("Icons.Visibility", new IMAGE_BRUSH_SVG("Common/visibility", Icon32x32));

		Style->Set("Icons.Extension", new IMAGE_BRUSH_SVG("Common/extension", Icon32x32));
	}
	
	// Buttons
	{
		Style->Set("Buttons.Default", DefaultButtonStyle);
	
		Style->Set("Buttons.Primary", PrimaryButtonStyle);
		
		Style->Set("Buttons.NoBorder", NoBorderButtonStyle);

		Style->Set("Buttons.DefaultNoBorder", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.15f, 0.15f, 0.15f)))
		);

		Style->Set("Buttons.Tab", FButtonStyle(NoBorderButtonStyle)
			.SetNormalForeground(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetHoveredForeground(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);
	}
	
	// CheckBoxes
	{
		Style->Set("CheckBoxes.DefaultNoBorder", FCheckBoxStyle()
			.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
			.SetUncheckedImage(FSlateColorBrush(FStyleColors::Transparent))
			.SetUncheckedHoveredImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetUncheckedPressedImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.15f, 0.15f, 0.15f)))
			.SetCheckedImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.f, 0.4f, 1.f)))
			.SetCheckedHoveredImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.f, 0.4f, 1.f)))
			.SetCheckedPressedImage(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.f, 0.4f, 1.f)))
		);
	}

	// TableRows
	{
		Style->Set("TableRows.Lists.Default", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);

		Style->Set("TableRows.Lists.Tab", FTableRowStyle(NormalTableRowStyle)
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
		
		Style->Set("TableRows.Tiles.Default", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetSelectorFocusedBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetActiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
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
