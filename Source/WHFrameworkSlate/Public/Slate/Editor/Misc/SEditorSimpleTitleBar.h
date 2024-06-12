// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

class WHFRAMEWORKSLATE_API SEditorSimpleTitleBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEditorSimpleTitleBar)
        : _TitleFont(FCoreStyle::GetDefaultFontStyle("Regular", 12))
        , _TitleColor(FLinearColor(0.9f, 0.9f, 0.9f))

    {}
        SLATE_ATTRIBUTE(FText, Title)

        SLATE_ATTRIBUTE(FSlateFontInfo, TitleFont)

        SLATE_ATTRIBUTE(FSlateColor, TitleColor)
        
        SLATE_ARGUMENT(TSharedPtr<SEditorWidgetBase>, ParentWidget)
    
    SLATE_END_ARGS()

    SEditorSimpleTitleBar();

    void Construct(const FArguments& InArgs);

protected:
    FReply OnCloseButtonClicked();

private:
    TSharedPtr<SEditorWidgetBase> ParentWidget;
};
