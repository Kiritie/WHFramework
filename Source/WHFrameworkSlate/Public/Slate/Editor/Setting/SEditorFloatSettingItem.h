// Copyright MetaApp, Inc. All Rights Reserved.
// Author Hao Wu
// CreateAt 2022-11-21 14:27

#pragma once

#include "CoreMinimal.h"
#include "SEditorSettingItemBase.h"

class WHFRAMEWORKSLATE_API SEditorFloatSettingItem : public SEditorSettingItemBase
{
public:
	struct FArguments : public TEditorSettingItemBaseNamedArgs<SEditorFloatSettingItem>
	{
	};

	SEditorFloatSettingItem();

	void Construct(const FArguments& InArgs);

public:
	virtual void OnRefresh() override;
};
