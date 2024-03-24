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

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

TSharedRef< ISlateStyle > FWHFrameworkSlateStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("WHFrameworkSlateStyle"));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("WHFramework"));

	if(Plugin)
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Content/Slate"));
	}
	else
	{
		Style->SetContentRoot(FPaths::ProjectContentDir() / TEXT("Slate"));
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

	const FTextBlockStyle NormalText = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Regular", 9))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D::ZeroVector)
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f))
		.SetHighlightShape(BOX_BRUSH("Common/TextBlockHighlightShape", FMargin(3.f/8.f)));

	const FTableRowStyle& NormalTableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

	const FButtonStyle& NoBorderButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>("NoBorder");

	// Common
	{
		Style->Set("Icons.White", new IMAGE_BRUSH("Common/white-1x1", FVector2D(1.f, 1.f)));
	
		Style->Set("Icons.Border", new BOX_BRUSH("Common/border-10x10", FMargin(0.1f)));

		Style->Set("Icons.Fillet_Border", new BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f)));

		Style->Set("Icons.Arrow_Up", new IMAGE_BRUSH("Common/arrow-up", FVector2D(12.f, 7.f)));
	
		Style->Set("Icons.Arrow_Down", new IMAGE_BRUSH("Common/arrow-down", FVector2D(12.f, 7.f)));

		Style->Set("Icons.Download", new IMAGE_BRUSH_SVG("Common/download", Icon16x16));
		
		Style->Set("Icons.Close", new IMAGE_BRUSH("Common/close", Icon16x16));

		Style->Set("Icons.Clear", new IMAGE_BRUSH("Common/close", Icon16x16));

		Style->Set("Icons.Setting", new IMAGE_BRUSH("Common/setting", Icon32x32));

		Style->Set("Icons.Extension", new IMAGE_BRUSH("Common/extension", FVector2D(14.f, 4.f)));
	
		Style->Set("Button.NoBorder", FButtonStyle(NoBorderButtonStyle));
	
		Style->Set("Button.NoBorder_1", FButtonStyle(NoBorderButtonStyle)
			.SetHovered(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.2f, 0.2f, 0.2f)))
			.SetPressed(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.15f, 0.15f, 0.15f)))
		);
	
		Style->Set("Button.Setting", FButtonStyle()
			.SetNormal(IMAGE_BRUSH("Common/setting", Icon32x32, FLinearColor(0.6f, 0.6f, 0.6f)))
			.SetHovered(IMAGE_BRUSH("Common/setting", Icon32x32, FLinearColor(0.75f, 0.75f, 0.75f)))
			.SetPressed(IMAGE_BRUSH("Common/setting", Icon32x32, FLinearColor(0.5f, 0.5f, 0.5f)))
		);
	}

	// Main
	{
		Style->Set("Main.List.Tab", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetSelectorFocusedBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveBrush(FSlateNoResource())
			.SetActiveHoveredBrush(FSlateNoResource())
			.SetInactiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);

		Style->Set("Main.List.Download", FTableRowStyle(NormalTableRowStyle)
			.SetEvenRowBackgroundBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetEvenRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Transparent))
			.SetOddRowBackgroundHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetSelectorFocusedBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetActiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetActiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetInactiveBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.02f, 0.02f, 0.02f)))
			.SetInactiveHoveredBrush(BOX_BRUSH("Common/fillet-border-4x4", FMargin(0.1f), FLinearColor(0.05f, 0.05f, 0.05f)))
			.SetTextColor(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetSelectedTextColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);
	
		Style->Set("Main.Button.Tab", FButtonStyle(NoBorderButtonStyle)
			.SetNormalForeground(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			.SetHoveredForeground(FSlateColor(FLinearColor(1.f, 1.f, 1.f)))
		);
	}

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef DEFAULT_FONT
