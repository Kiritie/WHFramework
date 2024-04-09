#pragma once
#include "Slate/Editor/Base/SEditorWidgetBase.h"
#include "Slate/Editor/Tab/SEditorTabListRow.h"

class SEditorSoftLabelBar;

class WHFRAMEWORKSLATE_API SEditorTabWidgetBase : public SEditorWidgetBase
{
	SLATE_BEGIN_ARGS(SEditorTabWidgetBase)
		:_ShowSplitLine(false),
		_LabelHeight(50.f)
	{}

		SLATE_ARGUMENT(bool, ShowSplitLine)
		SLATE_ATTRIBUTE(FOptionalSize, LabelHeight)
		SLATE_NAMED_SLOT(FArguments, LabelContent)
		SLATE_EVENT(FOnClicked, OnLabelClicked)
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	SEditorTabWidgetBase();

	void Construct(const FArguments& InArgs, const TSharedPtr<FEditorTabListItem>& InListItem);

public:
	static FName WidgetName;

protected:
	virtual void OnOpen(bool bInstant) override;
	
	virtual void OnClose(bool bInstant) override;

	virtual void OnRefresh() override;

protected:
	TSharedPtr<FEditorTabListItem> TabListItem;

public:
	TSharedPtr<FEditorTabListItem> GetTabListItem() { return TabListItem; };
};
