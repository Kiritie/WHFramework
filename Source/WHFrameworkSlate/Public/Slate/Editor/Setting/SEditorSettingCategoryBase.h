// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"

template <typename InWidgetType>
struct TEditorSettingCategoryBaseNamedArgs : public TSlateBaseNamedArgs<InWidgetType>
{
	typedef InWidgetType WidgetType;
	typedef typename WidgetType::FArguments WidgetArgsType;

	FORCENOINLINE TEditorSettingCategoryBaseNamedArgs()
		: _ShowSplitLine(false)
		, _Padding(FMargin(0.f))
	{
	}

	SLATE_ARGUMENT(bool, ShowSplitLine)
	SLATE_ATTRIBUTE(FMargin, Padding)
	SLATE_ATTRIBUTE(FText, Label)
};

class WHFRAMEWORKSLATE_API SEditorSettingCategoryBase : public SCompoundWidget
{
public:
	struct FArguments : public TEditorSettingCategoryBaseNamedArgs<SEditorSettingCategoryBase>
	{
		SLATE_DEFAULT_SLOT(WidgetArgsType, Content)
	};

	SEditorSettingCategoryBase();

	void Construct(const FArguments& InArgs);
};
