// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"

struct WHFRAMEWORKSLATE_API FEditorSettingItem
{
public:
    FEditorSettingItem()
    {
        ItemName = NAME_None;
        ItemLabel = FText::GetEmpty();
        ItemValue = FParameter();
    }

    FEditorSettingItem(const FName& ItemName, const FText& ItemLabel, const FParameter& ItemValue)
        : ItemName(ItemName)
        , ItemLabel(ItemLabel)
        , ItemValue(ItemValue)
    {
    }

public:
    FName ItemName;
    FText ItemLabel;
    FParameter ItemValue;
};

DECLARE_DELEGATE_TwoParams(FOnEditorSettingItemValueChanged, FName/* ItemName*/, FParameter/* InValue*/);

class WHFRAMEWORKSLATE_API SEditorSettingItemBase : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEditorSettingItemBase)
        : _Padding(FMargin(0.f))
    {}

		SLATE_ATTRIBUTE(FMargin, Padding)
        SLATE_ARGUMENT(FEditorSettingItem, SettingItem)
        SLATE_EVENT(FOnEditorSettingItemValueChanged, OnValueChanged)
        SLATE_NAMED_SLOT(FArguments, DescContent)
        SLATE_DEFAULT_SLOT(FArguments, Content)

    SLATE_END_ARGS()

    SEditorSettingItemBase();

    void Construct(const FArguments& InArgs);

public:
    virtual void OnRefresh();

public:
    void Refresh();

protected:
    FName SettingName;
    
    FParameter SettingValue;

    FOnEditorSettingItemValueChanged OnValueChanged;

public:
    virtual FName GetSettingName() const;
    
    virtual FParameter GetSettingValue() const;

	virtual void SetSettingValue(const FParameter& InValue);
};
