// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/EditorSlateWidgetStyleBase.h"

FEditorSlateWidgetStyle::FEditorSlateWidgetStyle()
{
}

FEditorSlateWidgetStyle::~FEditorSlateWidgetStyle()
{
}

const FName FEditorSlateWidgetStyle::TypeName(TEXT("FEditorSlateWidgetStyle"));

const FEditorSlateWidgetStyle& FEditorSlateWidgetStyle::GetDefault()
{
	static FEditorSlateWidgetStyle Default;
	return Default;
}

void FEditorSlateWidgetStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

