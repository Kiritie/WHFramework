#include "SWidgetStyleCreationDialog.h"

#include "SWidgetThemePicker.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Styling/AppStyle.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

void SWidgetStyleCreationDialog::Construct(const FArguments& InArgs)
{
	Theme = InArgs._Theme;
	Operation = InArgs._Operation;
	ParentTag = InArgs._ParentTag;
	Window = InArgs._Window;
	bTypeLocked = FWidgetThemeEditorService::Kind(ParentTag, Kind);
	if (!bTypeLocked)
	{
		Kind = EWidgetStyleKind::Button;
		ParentTag = WidgetStyleTags::Root(Kind);
	}
	OnCompleted = InArgs._OnCompleted;
	bInheritParent = bTypeLocked && !WidgetStyleTags::IsRoot(ParentTag);
	for (const FString& Root : FWidgetThemeEditorService::GetCreationRoots())
	{
		TSharedPtr<FString> Item = MakeShared<FString>(Root);
		CreationRoots.Add(Item);
		if (Theme.IsValid() && Theme->GetOutermost()->GetName().StartsWith(FPaths::GetPath(Root) + TEXT("/")))
		{
			CreationRoot = Item;
		}
	}
	if (!CreationRoot)
	{
		CreationRoot = CreationRoots[0];
	}
	auto Row = [](const TCHAR* Label, TSharedRef<SWidget> Control) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox) +
			   SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SBox).WidthOverride(110)[SNew(STextBlock).Text(FText::FromString(Label))]] +
			   SHorizontalBox::Slot().FillWidth(1).Padding(4)[Control];
	};
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight()[Row(TEXT("Name"), SAssignNew(NameInput, SEditableTextBox)
													   .OnTextChanged_Lambda(
														   [this](const FText& Text)
														   {
															   FString Error;
															   FWidgetThemeEditorService::ValidateName(Text.ToString(), Error);
															   NameInput->SetError(FText::FromString(Error));
														   }))];
	TSharedRef<SHorizontalBox> Types = SNew(SHorizontalBox).IsEnabled(!bTypeLocked);
	for (EWidgetStyleKind Type : {EWidgetStyleKind::Button, EWidgetStyleKind::Text, EWidgetStyleKind::Brush})
	{
		const FString Label = Type == EWidgetStyleKind::Button ? TEXT("Button") : Type == EWidgetStyleKind::Text ? TEXT("Text") : TEXT("Brush");
		Types->AddSlot().FillWidth(1)[SNew(SCheckBox)
										  .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
										  .IsChecked_Lambda([this, Type] { return Kind == Type ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
										  .OnCheckStateChanged_Lambda(
											  [this, Type](ECheckBoxState State)
											  {
												  if (State == ECheckBoxState::Checked)
												  {
													  Kind = Type;
													  ParentTag = WidgetStyleTags::Root(Type);
													  SourceTag = FGameplayTag();
													  bInheritParent = false;
												  }
											  })[SNew(STextBlock).Text(FText::FromString(Label))]];
	}
	Body->AddSlot().AutoHeight()[Row(TEXT("Type"), Types)];
	Body->AddSlot()
		.AutoHeight()[Row(TEXT("Parent Tag"), SNew(SComboButton)
												  .IsEnabled(bTypeLocked)
												  .OnGetMenuContent(this, &SWidgetStyleCreationDialog::BuildParentMenu)
												  .ButtonContent()[SNew(STextBlock).Text_Lambda([this] { return FText::FromName(ParentTag.GetTagName()); })])];
	if (Operation == EWidgetStyleOperation::Create)
	{
		Body->AddSlot().AutoHeight()[Row(
			TEXT("Create in"),
			SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&CreationRoots)
				.InitiallySelectedItem(CreationRoot)
				.OnGenerateWidget_Lambda(
					[](TSharedPtr<FString> Root)
					{ return SNew(STextBlock).Text(FText::FromString((*Root == TEXT("/Game/Themes") ? TEXT("Game | ") : TEXT("Plugin | ")) + *Root)); })
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Root, ESelectInfo::Type)
										   { CreationRoot = Root; })[SNew(STextBlock).Text_Lambda([this] { return FText::FromString(*CreationRoot); })])];
		TSharedRef<SHorizontalBox> Inheritance = SNew(SHorizontalBox);
		for (bool Inherit : {false, true})
		{
			Inheritance->AddSlot().FillWidth(
				1)[SNew(SCheckBox)
					   .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					   .IsEnabled_Lambda([this, Inherit] { return !Inherit || !WidgetStyleTags::IsRoot(ParentTag); })
					   .IsChecked_Lambda([this, Inherit] { return bInheritParent == Inherit ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					   .OnCheckStateChanged_Lambda(
						   [this, Inherit](ECheckBoxState State)
						   {
							   if (State == ECheckBoxState::Checked)
							   {
								   bInheritParent = Inherit;
							   }
						   })[SNew(STextBlock).Text(FText::FromString(Inherit ? TEXT("Parent Tag") : TEXT("None")))]];
		}
		Body->AddSlot().AutoHeight()[Row(TEXT("Inheritance"), Inheritance)];
	}
	else
	{
		Body->AddSlot().AutoHeight()[Row(TEXT("Source theme"), SNew(SWidgetThemePicker)
																   .AllowProjectDefault(false)
																   .SelectedTheme_Lambda([this] { return SourceTheme.Get(); })
																   .OnThemeSelected_Lambda(
																	   [this](UWidgetTheme* Source)
																	   {
																		   SourceTheme = Source;
																		   SourceTag = FGameplayTag();
																	   }))];
		Body->AddSlot().AutoHeight()[Row(TEXT("Source Tag"),
										 SNew(SComboButton)
											 .IsEnabled_Lambda([this] { return SourceTheme.IsValid(); })
											 .OnGetMenuContent(this, &SWidgetStyleCreationDialog::BuildSourceMenu)
											 .ButtonContent()[SNew(STextBlock).Text_Lambda([this] { return FText::FromName(SourceTag.GetTagName()); })])];
	}
	Body->AddSlot().FillHeight(1).Padding(4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return FText::FromString(Report); })];
	Body->AddSlot().AutoHeight().Padding(4)[SNew(SButton)
												.Text(NSLOCTEXT("WidgetTheme", "Confirm", "Confirm"))
												.IsEnabled_Lambda(
													[this]
													{
														FString Error;
														return FWidgetThemeEditorService::ValidateName(NameInput->GetText().ToString(), Error) &&
															   (Operation == EWidgetStyleOperation::Create || (SourceTheme.IsValid() && SourceTag.IsValid()));
													})
												.OnClicked(this, &SWidgetStyleCreationDialog::Execute)];
	ChildSlot[SNew(SBox).Padding(12)[Body]];
}

TSharedRef<SWidget> SWidgetStyleCreationDialog::BuildSourceMenu()
{
	FMenuBuilder Menu(true, nullptr);
	for (FGameplayTag StyleTag : FWidgetThemeEditorService::Tags(SourceTheme.Get()))
	{
		EWidgetStyleKind Type;
		if (FWidgetThemeEditorService::Kind(StyleTag, Type) && Type == Kind)
		{
			Menu.AddMenuEntry(FText::FromName(StyleTag.GetTagName()), FText::FromString(FWidgetThemeEditorService::DescribeTag(SourceTheme.Get(), StyleTag)),
							  FSlateIcon(), FUIAction(FExecuteAction::CreateSPLambda(this, [this, StyleTag] { SourceTag = StyleTag; })));
		}
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SWidgetStyleCreationDialog::BuildParentMenu()
{
	FMenuBuilder Menu(true, nullptr);
	TArray<FGameplayTag> Parents{WidgetStyleTags::Root(Kind)};
	Parents.Append(FWidgetThemeEditorService::Tags(Theme.Get()));
	for (FGameplayTag StyleTag : Parents)
	{
		EWidgetStyleKind Type;
		if (FWidgetThemeEditorService::Kind(StyleTag, Type) && Type == Kind)
		{
			Menu.AddMenuEntry(FText::FromName(StyleTag.GetTagName()), FText(), FSlateIcon(),
							  FUIAction(FExecuteAction::CreateSPLambda(this,
																	   [this, StyleTag]
																	   {
																		   ParentTag = StyleTag;
																		   if (WidgetStyleTags::IsRoot(StyleTag))
																		   {
																			   bInheritParent = false;
																		   }
																	   })));
		}
	}
	return Menu.MakeWidget();
}

FReply SWidgetStyleCreationDialog::Execute()
{
	Report.Reset();
	const FString Name = NameInput->GetText().ToString();
	const bool Good = Operation == EWidgetStyleOperation::Create
						  ? FWidgetThemeEditorService::Create(Theme.Get(), ParentTag, Name, Kind, *CreationRoot, bInheritParent, Report)
						  : FWidgetThemeEditorService::Associate(Theme.Get(), ParentTag, Name, SourceTheme.Get(), SourceTag, Report);
	if (Good)
	{
		OnCompleted.ExecuteIfBound();
		if (TSharedPtr<SWindow> Dialog = Window.Pin())
		{
			Dialog->RequestDestroyWindow();
		}
	}
	return FReply::Handled();
}
