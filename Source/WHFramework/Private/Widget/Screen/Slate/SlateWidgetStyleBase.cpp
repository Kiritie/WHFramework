// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Screen/Slate/SlateWidgetStyleBase.h"

FSlateStyleBase::FSlateStyleBase()
{
}

FSlateStyleBase::~FSlateStyleBase()
{
}

const FName FSlateStyleBase::TypeName(TEXT("FSlateStyleBase"));

const FSlateStyleBase& FSlateStyleBase::GetDefault()
{
	static FSlateStyleBase Default;
	return Default;
}

void FSlateStyleBase::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

