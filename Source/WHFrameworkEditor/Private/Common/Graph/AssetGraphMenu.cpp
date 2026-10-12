#include "Common/Graph/AssetGraphMenu.h"

#include "Framework/Commands/GenericCommands.h"

void FAssetGraphMenu::AddEditing(FMenuBuilder& Menu, FNewMenuDelegate TypePicker, FCanExecuteAction CanEdit, FExecuteAction OpenSource, bool bReferencedAsset)
{
	Menu.AddSubMenu(NSLOCTEXT("AssetGraphMenu", "Editing", "Editing"), FText::GetEmpty(),
		FNewMenuDelegate::CreateLambda([TypePicker, CanEdit, OpenSource, bReferencedAsset](FMenuBuilder& Editing)
		{
			Editing.AddSubMenu(NSLOCTEXT("AssetGraphMenu", "ChangeType", "Change Node Type"),
				NSLOCTEXT("AssetGraphMenu", "ChangeTypeTip", "Keep identity, connections and compatible properties"),
				TypePicker, FUIAction(FExecuteAction(), CanEdit), NAME_None, EUserInterfaceActionType::Button);
			Editing.AddMenuEntry(bReferencedAsset ? NSLOCTEXT("AssetGraphMenu", "OpenReference", "Open Referenced Task Asset") :
				NSLOCTEXT("AssetGraphMenu", "OpenSource", "Open Blueprint or Code"), FText::GetEmpty(), FSlateIcon(), FUIAction(OpenSource));
		}));
}

void FAssetGraphMenu::AddCommon(FMenuBuilder& Menu, FUIAction AutoLayout, FExecuteAction FrameAll, FExecuteAction AssetDetails)
{
	Menu.BeginSection("Edit", NSLOCTEXT("AssetGraphMenu", "Edit", "Edit"));
	Menu.AddMenuEntry(FGenericCommands::Get().Undo);
	Menu.AddMenuEntry(FGenericCommands::Get().Redo);
	Menu.AddMenuEntry(FGenericCommands::Get().Cut);
	Menu.AddMenuEntry(FGenericCommands::Get().Copy);
	Menu.AddMenuEntry(FGenericCommands::Get().Paste);
	Menu.AddMenuEntry(FGenericCommands::Get().Duplicate);
	Menu.AddMenuEntry(FGenericCommands::Get().Delete);
	Menu.AddMenuEntry(FGenericCommands::Get().SelectAll);
	Menu.EndSection();
	Menu.BeginSection("Graph", NSLOCTEXT("AssetGraphMenu", "Graph", "Graph"));
	Menu.AddMenuEntry(NSLOCTEXT("AssetGraphMenu", "Layout", "Auto Layout"),
		NSLOCTEXT("AssetGraphMenu", "LayoutTip", "Arrange nodes while preserving execution order"), FSlateIcon(), AutoLayout);
	Menu.AddMenuEntry(NSLOCTEXT("AssetGraphMenu", "Frame", "Frame All"), FText::GetEmpty(), FSlateIcon(), FUIAction(FrameAll));
	Menu.AddMenuEntry(NSLOCTEXT("AssetGraphMenu", "AssetDetails", "Asset Details"), FText::GetEmpty(), FSlateIcon(), FUIAction(AssetDetails));
	Menu.EndSection();
}

FText FAssetGraphMenu::GetDisconnectPinLabel()
{
	return NSLOCTEXT("AssetGraphMenu", "DisconnectPin", "Disconnect Pin Links");
}

FText FAssetGraphMenu::GetDisconnectNodeLabel()
{
	return NSLOCTEXT("AssetGraphMenu", "DisconnectNode", "Disconnect Node Links");
}
