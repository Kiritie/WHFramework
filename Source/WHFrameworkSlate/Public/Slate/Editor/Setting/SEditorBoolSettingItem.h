// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "SEditorSettingItemBase.h"

class WHFRAMEWORKSLATE_API SEditorBoolSettingItem : public SEditorSettingItemBase
{
public:
    SLATE_BEGIN_ARGS(SEditorBoolSettingItem)
    {}

		SLATE_ATTRIBUTE(FMargin, Padding)
        SLATE_ARGUMENT(FEditorSettingItem, SettingItem)
        SLATE_EVENT(FOnEditorSettingItemValueChanged, OnValueChanged)

    SLATE_END_ARGS()

    SEditorBoolSettingItem();

    void Construct(const FArguments& InArgs);

public:
    virtual void OnRefresh() override;
};
