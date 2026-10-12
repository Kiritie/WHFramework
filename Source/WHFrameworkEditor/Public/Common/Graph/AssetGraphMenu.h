#pragma once

#include "Framework/MultiBox/MultiBoxBuilder.h"

class WHFRAMEWORKEDITOR_API FAssetGraphMenu
{
public:
	static void AddEditing(FMenuBuilder& Menu, FNewMenuDelegate TypePicker, FCanExecuteAction CanEdit, FExecuteAction OpenSource, bool bReferencedAsset = false);
	static void AddCommon(FMenuBuilder& Menu, FUIAction AutoLayout, FExecuteAction FrameAll, FExecuteAction AssetDetails);
	static FText GetDisconnectPinLabel();
	static FText GetDisconnectNodeLabel();
};
