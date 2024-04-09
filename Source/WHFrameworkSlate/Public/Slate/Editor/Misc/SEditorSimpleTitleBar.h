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
    {}
        SLATE_ARGUMENT(FText, Title)
        SLATE_ARGUMENT(TSharedPtr<SEditorWidgetBase>, ParentWidget)
    
    SLATE_END_ARGS()

    SEditorSimpleTitleBar();

    void Construct(const FArguments& InArgs);

protected:
    FReply OnCloseButtonClicked();

private:
    TSharedPtr<SEditorWidgetBase> ParentWidget;
};
