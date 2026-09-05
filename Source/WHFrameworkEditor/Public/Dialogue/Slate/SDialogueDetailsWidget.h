#pragma once

#include "SSingleObjectDetailsPanel.h"

class FDialogueEditor;

class SDialogueDetailsWidget : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SDialogueDetailsWidget) {}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs, TSharedPtr<FDialogueEditor> InEditor);
	virtual UObject* GetObjectToObserve() const override;
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;

private:
	TWeakPtr<FDialogueEditor> Editor;
};
