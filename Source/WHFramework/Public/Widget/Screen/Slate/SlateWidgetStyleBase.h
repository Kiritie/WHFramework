// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateWidgetStyleContainerBase.h"

#include "SlateWidgetStyleBase.generated.h"

/**
 * 
 */
USTRUCT()
struct WHFRAMEWORK_API FSlateStyleBase : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FSlateStyleBase();
	virtual ~FSlateStyleBase();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FSlateStyleBase& GetDefault();
};

/**
 */
UCLASS(hidecategories=Object, MinimalAPI)
class USlateWidgetStyleBase : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	/** The actual data describing the widget appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FSlateStyleBase WidgetStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &WidgetStyle );
	}
};
