// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

class WHFRAMEWORKSLATE_API SEditorSimpleWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEditorSimpleWindow)
        : _TitleFont(FCoreStyle::GetDefaultFontStyle("Regular", 13))
        , _TitleColor(FLinearColor(0.9f, 0.9f, 0.9f))
        , _WindowWidth(800.f)
        , _WindowHeight(600.f)
        , _ContentPadding(FMargin(0.f))

    {}
        SLATE_ATTRIBUTE(FText, Title)

        SLATE_ATTRIBUTE(FSlateFontInfo, TitleFont)

        SLATE_ATTRIBUTE(FSlateColor, TitleColor)
        
        SLATE_ATTRIBUTE(FOptionalSize, WindowWidth)
        
        SLATE_ATTRIBUTE(FOptionalSize, WindowHeight)
           
        SLATE_ATTRIBUTE(FMargin, ContentPadding)

        SLATE_DEFAULT_SLOT(FArguments, Content)
 
        SLATE_ARGUMENT(TSharedPtr<SEditorWidgetBase>, ParentWidget)

    SLATE_END_ARGS()

    SEditorSimpleWindow();

    void Construct(const FArguments& InArgs);

private:
    TSharedPtr<SEditorWidgetBase> ParentWidget;
};
