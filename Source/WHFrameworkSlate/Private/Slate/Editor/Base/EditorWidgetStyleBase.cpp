// Fill out your copyright notice in the Description page of Project Settings.


#include "Slate/Editor/Base/EditorWidgetStyleBase.h"

FEditorWidgetStyle::FEditorWidgetStyle()
{
}

FEditorWidgetStyle::~FEditorWidgetStyle()
{
}

const FName FEditorWidgetStyle::TypeName(TEXT("FEditorSlateWidgetStyle"));

const FEditorWidgetStyle& FEditorWidgetStyle::GetDefault()
{
	static FEditorWidgetStyle Default;
	return Default;
}

void FEditorWidgetStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

