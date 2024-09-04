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
		ItemValue = FParameter();
	}

	FEditorSettingItem(const FName& ItemName, const FParameter& ItemValue)
		: ItemName(ItemName)
		, ItemValue(ItemValue)
	{
	}

public:
	FName ItemName;
	FParameter ItemValue;
};

DECLARE_DELEGATE_TwoParams(FOnEditorSettingItemValueChanged, const FName&/* ItemName*/, const FParameter&/* InValue*/);

template <typename InWidgetType>
struct TEditorSettingItemBaseNamedArgs : public TSlateBaseNamedArgs<InWidgetType>
{
	typedef InWidgetType WidgetType;
	typedef typename WidgetType::FArguments WidgetArgsType;

	FORCENOINLINE TEditorSettingItemBaseNamedArgs()
		: _Padding(FMargin(0.f))
	{
	}

	SLATE_ATTRIBUTE(FMargin, Padding)
	SLATE_ARGUMENT(FEditorSettingItem, SettingItem)
	SLATE_EVENT(FOnEditorSettingItemValueChanged, OnValueChanged)
};

class WHFRAMEWORKSLATE_API SEditorSettingItemBase : public SCompoundWidget
{
public:
	struct FArguments : public TEditorSettingItemBaseNamedArgs<SEditorSettingItemBase>
	{
		SLATE_NAMED_SLOT(WidgetArgsType, DescContent)
		SLATE_DEFAULT_SLOT(WidgetArgsType, Content)
	};

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
