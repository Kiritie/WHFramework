#pragma once

#include "Editor/PropertyEditor/Public/IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"

class UDialogueAsset;
struct FDialogueEditorState;

class FDialogueCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(TSharedRef<FDialogueEditorState> InState);
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;
	void TextCommited(const FText& InText, ETextCommit::Type InCommitType, UDialogueAsset* Dialogue, int32 id);

private:
	TSharedPtr<FDialogueEditorState> EditingState;
	//FReply RefreshOnClicked();

	/** Associated detail layout builder */
	IDetailLayoutBuilder* DetailLayoutBuilder;
};
