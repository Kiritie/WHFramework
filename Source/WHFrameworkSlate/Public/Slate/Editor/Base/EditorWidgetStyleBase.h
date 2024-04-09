// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateWidgetStyleContainerBase.h"

#include "EditorWidgetStyleBase.generated.h"

/**
 * 
 */
USTRUCT()
struct WHFRAMEWORKSLATE_API FEditorWidgetStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FEditorWidgetStyle();
	virtual ~FEditorWidgetStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FEditorWidgetStyle& GetDefault();
};

/**
 */
UCLASS(hidecategories=Object)
class WHFRAMEWORKSLATE_API UEditorWidgetStyleBase : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	/** The actual data describing the widget appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FEditorWidgetStyle WidgetStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &WidgetStyle );
	}
};
