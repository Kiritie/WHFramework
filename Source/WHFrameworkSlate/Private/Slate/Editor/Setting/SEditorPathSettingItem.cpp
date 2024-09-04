#include "Slate/Editor/Setting/SEditorPathSettingItem.h"

#include "WHFrameworkSlateStyle.h"
#include "Platform/PlatformManager.h"
#include "Slate/Editor/Misc/SEditorSplitLine.h"

SEditorPathSettingItem::SEditorPathSettingItem()
{
}

void SEditorPathSettingItem::Construct(const FArguments& InArgs)
{
	SEditorSettingItemBase::Construct(
		SEditorSettingItemBase::FArguments()
		.Padding(InArgs._Padding)
		.SettingItem(InArgs._SettingItem)
		.OnValueChanged(InArgs._OnValueChanged)
		.Content()
		[
			SNew(SBorder)
			.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Border_Radius_16"))
			.BorderBackgroundColor(FLinearColor(0.025f, 0.025f, 0.025f))
			.Padding(FMargin(20.f, 15.f))
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.FillWidth(1.f)
				.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
					.AutoHeight()
					[
						SNew(SEditableText)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.Visibility_Lambda([InArgs](){ return InArgs._Editable.Get() ? EVisibility::Visible : EVisibility::HitTestInvisible; })
						.Text_Lambda([this]()
						{
							return FText::FromString(GetSettingValue().GetStringValue());
						})
						.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type TextCommitType)
						{
							SetSettingValue(Val.ToString());
						})
					]
					
					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.AutoHeight()
					[
						SNew(SEditorSplitLine)
						.Height(1.f)
						.Color(FLinearColor(0.15f, 0.15f, 0.15f))
						.Visibility_Lambda([InArgs](){ return InArgs._Editable.Get() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					]
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(16.f)
					.HeightOverride(16.f)
					[
						SNew(SButton)
						.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Buttons.SelectPath"))
						.OnClicked_Lambda([this, InArgs]()
						{
							IPlatformManager* PlatformManager = FPlatformManager::GetPtr();
							
							TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
							void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

							FString Path;
							if(PlatformManager->OpenDirectoryDialog(ParentWindowHandle, GetSettingValue().GetStringValue(), GetSettingValue().GetStringValue(), Path))
							{
								SetSettingValue(Path + (!Path.EndsWith(TEXT("/")) ? TEXT("/") : TEXT("")) + InArgs._AdditionPath);
							}
							return FReply::Handled();
						})
					]
				]
			]
		]
	);
}

void SEditorPathSettingItem::OnRefresh()
{
}
