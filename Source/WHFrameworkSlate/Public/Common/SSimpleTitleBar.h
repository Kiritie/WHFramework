// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "Base/SEditorWidgetBase.h"

class WHFRAMEWORKSLATE_API SSimpleTitleBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSimpleTitleBar) 
    {}
        SLATE_ARGUMENT(FText, Title)
        SLATE_ARGUMENT(TSharedPtr<SEditorWidgetBase>, ParentWidget)
    
    SLATE_END_ARGS()

    SSimpleTitleBar();

    void Construct(const FArguments& InArgs);

protected:
    FReply OnCloseButtonClicked();

private:
    TSharedPtr<SEditorWidgetBase> ParentWidget;
};
