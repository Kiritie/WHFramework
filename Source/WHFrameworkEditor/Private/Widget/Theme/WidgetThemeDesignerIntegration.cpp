#include "Widget/Theme/WidgetThemeDesignerIntegration.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "TickableEditorObject.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SWidgetThemePicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "UObject/StrongObjectPtr.h"

namespace WidgetThemeDesigner
{
	struct FSession
	{
		TWeakObjectPtr<UWidgetBlueprint> Asset;
		TWeakObjectPtr<UUserWidget> Preview;
		TStrongObjectPtr<UWidgetTheme> Theme;
		TSharedPtr<FExtender> Toolbar;
		bool bExplicit = false;
		void Apply()
		{
			if (!Preview.IsValid())
			{
				return;
			}
			if (bExplicit && Theme.IsValid())
			{
				FWidgetThemeResolver::SetPreviewOverride(Preview.Get(), Theme.Get());
			}
			else
			{
				FWidgetThemeResolver::ClearPreviewOverride(Preview.Get());
			}
		}
		FText Label() const
		{
			const UWidgetTheme* Current = bExplicit ? Theme.Get() : FWidgetThemeResolver::ResolveTheme(nullptr);
			return FText::FromString((bExplicit ? TEXT("Theme: temporary override | ") : TEXT("Theme: project default | ")) + GetPathNameSafe(Current));
		}
		~FSession()
		{
			if (Preview.IsValid())
			{
				FWidgetThemeResolver::ClearPreviewOverride(Preview.Get());
			}
		}
	};

	class FIntegration final : public FTickableEditorObject
	{
		TMap<IAssetEditorInstance*, TSharedPtr<FSession>> Sessions;
		float Elapsed = 1;

	public:
		virtual TStatId GetStatId() const override
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FWidgetThemeDesignerIntegration, STATGROUP_Tickables);
		}
		virtual void Tick(float DeltaTime) override
		{
			Elapsed += DeltaTime;
			if (Elapsed < 0.25f || !GEditor)
			{
				return;
			}
			Elapsed = 0;
			UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			if (!Sub)
			{
				return;
			}
			TSet<IAssetEditorInstance*> Alive;
			for (UObject* Asset : Sub->GetAllEditedAssets())
			{
				UWidgetBlueprint* BP = Cast<UWidgetBlueprint>(Asset);
				if (!BP)
				{
					continue;
				}
				for (IAssetEditorInstance* Instance : Sub->FindEditorsForAsset(BP))
				{
					if (!Instance || Instance->GetEditorName() != FName(TEXT("WidgetBlueprintEditor")))
					{
						continue;
					}
					Alive.Add(Instance);
					auto* Editor = static_cast<FWidgetBlueprintEditor*>(Instance);
					TSharedPtr<FSession>& Session = Sessions.FindOrAdd(Instance);
					if (!Session)
					{
						Session = MakeShared<FSession>();
						Session->Asset = BP;
						Session->Toolbar = MakeShared<FExtender>();
						TWeakPtr<FSession> Weak = Session;
						Session->Toolbar->AddToolBarExtension(
							TEXT("Asset"), EExtensionHook::After, nullptr,
							FToolBarExtensionDelegate::CreateLambda(
								[Weak](FToolBarBuilder& Builder)
								{
									Builder.AddWidget(
										SNew(SComboButton)
											.ButtonContent()[SNew(STextBlock)
																 .Text_Lambda(
																	 [Weak]
																	 {
																		 auto S = Weak.Pin();
																		 return S ? S->Label() : FText::GetEmpty();
																	 })]
											.OnGetMenuContent_Lambda(
												[Weak]() -> TSharedRef<SWidget>
												{
													return SNew(SBox).WidthOverride(
														520)[SNew(SVerticalBox) +
															 SVerticalBox::Slot().AutoHeight().Padding(
																 4)[SNew(STextBlock)
																		.AutoWrapText(true)
																		.Text(FText::FromString(TEXT(
																			"Temporary preview for this Designer window. Does not change project settings, "
																			"widget defaults, another Designer, or PIE.")))] +
															 SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SWidgetThemePicker)
																											  .SelectedTheme_Lambda(
																												  [Weak]() -> UWidgetTheme*
																												  {
																													  auto Session = Weak.Pin();
																													  return Session ? Session->Theme.Get()
																																	 : nullptr;
																												  })
																											  .OnThemeSelected_Lambda(
																												  [Weak](UWidgetTheme* Theme)
																												  {
																													  if (auto Session = Weak.Pin())
																													  {
																														  Session->Theme.Reset(Theme);
																														  Session->bExplicit = Theme != nullptr;
																														  Session->Apply();
																													  }
																												  })] +
															 SVerticalBox::Slot().AutoHeight().Padding(
																 4)[SNew(SButton)
																		.Text(FText::FromString(TEXT("Restore project default")))
																		.OnClicked_Lambda(
																			[Weak]
																			{
																				if (auto S = Weak.Pin())
																				{
																					S->bExplicit = false;
																					S->Theme.Reset();
																					S->Apply();
																				}
																				return FReply::Handled();
																			})]];
												}));
								}));
						Editor->AddToolbarExtender(Session->Toolbar);
						Editor->RegenerateMenusAndToolbars();
					}
					UUserWidget* Preview = Editor->GetPreview();
					if (Session->Preview.Get() != Preview)
					{
						if (Session->Preview.IsValid())
						{
							FWidgetThemeResolver::ClearPreviewOverride(Session->Preview.Get());
						}
						Session->Preview = Preview;
						Session->Apply();
					}
				}
			}
			for (auto It = Sessions.CreateIterator(); It; ++It)
			{
				if (!Alive.Contains(It.Key()))
				{
					It.RemoveCurrent();
				}
			}
		}
		virtual ~FIntegration() override
		{
			if (GEditor)
			{
				const auto Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetAllOpenEditors();
				for (auto& Pair : Sessions)
				{
					if (Editors.Contains(Pair.Key))
					{
						auto* Editor = static_cast<FWidgetBlueprintEditor*>(Pair.Key);
						Editor->RemoveToolbarExtender(Pair.Value->Toolbar);
						Editor->RegenerateMenusAndToolbars();
					}
				}
			}
			Sessions.Reset();
		}
	};
	TUniquePtr<FIntegration> Integration;
} // namespace WidgetThemeDesigner
void RegisterWidgetThemeDesignerIntegration()
{
	if (!WidgetThemeDesigner::Integration)
	{
		WidgetThemeDesigner::Integration = MakeUnique<WidgetThemeDesigner::FIntegration>();
	}
}
void UnregisterWidgetThemeDesignerIntegration()
{
	WidgetThemeDesigner::Integration.Reset();
}
