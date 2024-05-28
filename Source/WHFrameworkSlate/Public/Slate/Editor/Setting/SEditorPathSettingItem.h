// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "SEditorSettingItemBase.h"

class WHFRAMEWORKSLATE_API SEditorPathSettingItem : public SEditorSettingItemBase
{
public:
    SLATE_BEGIN_ARGS(SEditorPathSettingItem)
    {}

		SLATE_ATTRIBUTE(FMargin, Padding)
        SLATE_ARGUMENT(FEditorSettingItem, SettingItem)
        SLATE_EVENT(FOnEditorSettingItemValueChanged, OnValueChanged)

    SLATE_END_ARGS()

    SEditorPathSettingItem();

    void Construct(const FArguments& InArgs);

public:
    virtual void OnRefresh() override;
};
