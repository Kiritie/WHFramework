#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widget/Theme/WidgetTheme.h"

class SWindow;
class SEditableTextBox;

enum class EWidgetStyleOperation : uint8
{
	None,
	Create,
	Associate
};

class SWidgetStyleCreationDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetStyleCreationDialog) {}
	SLATE_ARGUMENT(UWidgetTheme*, Theme)
	SLATE_ARGUMENT(FGameplayTag, ParentTag)
	SLATE_ARGUMENT(TSharedPtr<SWindow>, Window)
	SLATE_EVENT(FSimpleDelegate, OnCompleted)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);

private:
	TWeakObjectPtr<UWidgetTheme> Theme;
	TWeakObjectPtr<UWidgetTheme> SourceTheme;
	EWidgetStyleOperation Operation = EWidgetStyleOperation::Create;
	EWidgetStyleKind Kind = EWidgetStyleKind::Button;
	bool bTypeLocked = false;
	bool bInheritParent = false;
	FGameplayTag ParentTag;
	FGameplayTag SourceTag;
	TArray<TSharedPtr<EWidgetStyleKind>> StyleTypes;
	TArray<TSharedPtr<FString>> CreationRoots;
	TSharedPtr<FString> CreationRoot;
	TSharedPtr<SEditableTextBox> NameInput;
	TWeakPtr<SWindow> Window;
	FSimpleDelegate OnCompleted;
	FString Report;
	TSharedRef<SWidget> BuildSourceMenu();
	TSharedRef<SWidget> BuildParentMenu();
	FReply Execute();
};
