#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Tests/WidgetThemeTestTags.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetThemeFallbacks.h"
#include "Widget/Theme/WidgetThemeSubsystem.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestButton, "WH.Tests.Theme.Button");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestText, "WH.Tests.Theme.Text");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestBrush, "WH.Tests.Theme.Brush");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorParentA, "Style.Brush.ThemeEditorParentA");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorParentB, "Style.Brush.ThemeEditorParentB");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorChild, "Style.Brush.ThemeEditorParentA.Child");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorAlias, "Style.Brush.ThemeEditorAlias");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorButton, "Style.Button.ThemeEditorButton");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorText, "Style.Text.ThemeEditorText");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetThemeTypedMapTest, "WHFramework.WidgetTheme.TypedMapAndValidation",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetThemeTypedMapTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWidgetTheme> Theme(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UWidgetStyleCatalog> Catalog(NewObject<UWidgetStyleCatalog>());
	Catalog->Types.Add(TAG_ThemeTestButton, EWidgetStyleKind::Button);
	Catalog->Types.Add(TAG_ThemeTestText, EWidgetStyleKind::Text);
	Catalog->Types.Add(TAG_ThemeTestBrush, EWidgetStyleKind::Brush);
	FString Error;
	TestFalse(TEXT("Incomplete theme rejected"), FWidgetThemeResolver::Validate(Theme.Get(), Catalog.Get(), Error));
	TestFalse(TEXT("Text class cannot enter button map"),
			  Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Button, UWidgetFallbackTextStyle::StaticClass()));
	TestTrue(TEXT("Button class accepted"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Button, UWidgetFallbackButtonStyle::StaticClass()));
	TestFalse(TEXT("Duplicate tag across maps rejected"),
			  Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Text, UWidgetFallbackTextStyle::StaticClass()));
	TestTrue(TEXT("Text class accepted"), Theme->SetStyleClass(TAG_ThemeTestText, EWidgetStyleKind::Text, UWidgetFallbackTextStyle::StaticClass()));
	TestTrue(TEXT("Brush class accepted"), Theme->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UWidgetFallbackBrushStyle::StaticClass()));
	TestTrue(TEXT("Complete theme validates"), FWidgetThemeResolver::Validate(Theme.Get(), Catalog.Get(), Error));
	TestNull(TEXT("No implicit tag/type fallback"), Theme->GetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Text));
	Catalog->Types[TAG_ThemeTestButton] = EWidgetStyleKind::Text;
	TestFalse(TEXT("Global type mismatch rejected"), FWidgetThemeResolver::Validate(Theme.Get(), Catalog.Get(), Error));
	Catalog->Types[TAG_ThemeTestButton] = EWidgetStyleKind::Button;
	Theme->RemoveStyle(TAG_ThemeTestBrush, EWidgetStyleKind::Brush);
	TestFalse(TEXT("Missing configured mapping remains invalid"), FWidgetThemeResolver::Validate(Theme.Get(), Catalog.Get(), Error));
	TestTrue(TEXT("Button placeholder derives correct base"),
			 FWidgetThemeResolver::Placeholder(EWidgetStyleKind::Button)->IsChildOf(FWidgetThemeResolver::NativeBase(EWidgetStyleKind::Button)));
	TestTrue(TEXT("Text placeholder derives correct base"),
			 FWidgetThemeResolver::Placeholder(EWidgetStyleKind::Text)->IsChildOf(FWidgetThemeResolver::NativeBase(EWidgetStyleKind::Text)));
	TestTrue(TEXT("Brush placeholder derives correct base"),
			 FWidgetThemeResolver::Placeholder(EWidgetStyleKind::Brush)->IsChildOf(FWidgetThemeResolver::NativeBase(EWidgetStyleKind::Brush)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetThemePreviewScopeTest, "WHFramework.WidgetTheme.PreviewScope",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetThemePreviewScopeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UObject> RootA(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UObject> RootB(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UObject> ChildA(NewObject<UWidgetTheme>(RootA.Get()));
	TStrongObjectPtr<UObject> ChildB(NewObject<UWidgetTheme>(RootB.Get()));
	TStrongObjectPtr<UWidgetTheme> ThemeA(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UWidgetTheme> ThemeB(NewObject<UWidgetTheme>());
	FWidgetThemeResolver::SetPreviewOverride(RootA.Get(), ThemeA.Get());
	FWidgetThemeResolver::SetPreviewOverride(RootB.Get(), ThemeB.Get());
	TestTrue(TEXT("Preview A resolves A"), FWidgetThemeResolver::ResolveTheme(ChildA.Get()) == ThemeA.Get());
	TestTrue(TEXT("Preview B resolves B"), FWidgetThemeResolver::ResolveTheme(ChildB.Get()) == ThemeB.Get());
	UWorld* PIEWorld = UWorld::CreateWorld(EWorldType::PIE, false);
	TestNotNull(TEXT("PIE test world created"), PIEWorld);
	if (PIEWorld)
	{
		FWidgetThemeResolver::SetPreviewOverride(PIEWorld, ThemeA.Get());
		TestNull(TEXT("PIE without GI cannot adopt preview A"), FWidgetThemeResolver::ResolveTheme(PIEWorld));
		PIEWorld->DestroyWorld(false);
	}
	FWidgetThemeResolver::ClearPreviewOverride(RootA.Get());
	TestTrue(TEXT("Closing A does not clear B"), FWidgetThemeResolver::ResolveTheme(ChildB.Get()) == ThemeB.Get());
	FWidgetThemeResolver::ClearPreviewOverride(RootB.Get());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetThemeSwitchOwnershipTest, "WHFramework.WidgetTheme.SwitchOwnershipAndFailure",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetThemeSwitchOwnershipTest::RunTest(const FString& Parameters)
{
	auto* Settings = GetMutableDefault<UWidgetThemeSettings>();
	const auto SavedDefault = Settings->DefaultTheme;
	const auto SavedAvailable = Settings->AvailableThemes;
	const auto SavedCatalog = Settings->StyleCatalog;
	TStrongObjectPtr<UWidgetStyleCatalog> Catalog(NewObject<UWidgetStyleCatalog>());
	TStrongObjectPtr<UWidgetTheme> ThemeA(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UWidgetTheme> ThemeB(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UGameInstance> OwnerA(NewObject<UGameInstance>());
	TStrongObjectPtr<UGameInstance> OwnerB(NewObject<UGameInstance>());
	TStrongObjectPtr<UWidgetThemeSubsystem> InstanceA(NewObject<UWidgetThemeSubsystem>(OwnerA.Get()));
	TStrongObjectPtr<UWidgetThemeSubsystem> InstanceB(NewObject<UWidgetThemeSubsystem>(OwnerB.Get()));
	Catalog->Types.Add(TAG_ThemeTestBrush, EWidgetStyleKind::Brush);
	ThemeA->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UWidgetFallbackBrushStyle::StaticClass());
	ThemeB->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UWidgetFallbackBrushStyle::StaticClass());
	Settings->DefaultTheme = ThemeA.Get();
	Settings->AvailableThemes = {ThemeA.Get(), ThemeB.Get()};
	Settings->StyleCatalog = Catalog.Get();
	FString Error;
	TestTrue(TEXT("Instance A commits A"), InstanceA->ApplyLoadedTheme(ThemeA.Get(), Error));
	TestTrue(TEXT("Instance B commits B"), InstanceB->ApplyLoadedTheme(ThemeB.Get(), Error));
	const uint64 PreviousSerial = InstanceA->RequestSerial;
	TestTrue(TEXT("New switch commits B"), InstanceA->ApplyLoadedTheme(ThemeB.Get(), Error));
	InstanceA->CompleteRequest(PreviousSerial, ThemeA.Get(), Catalog.Get());
	TestEqual(TEXT("Stale completion cannot replace the latest theme"), InstanceA->GetActiveTheme(), ThemeB.Get());
	TestFalse(TEXT("Missing theme is rejected"), InstanceA->ApplyLoadedTheme(nullptr, Error));
	TestEqual(TEXT("Rejected request retains previous active theme"), InstanceA->GetActiveTheme(), ThemeB.Get());
	Settings->StyleCatalog = nullptr;
	TestFalse(TEXT("Missing catalog is rejected"), InstanceA->ApplyLoadedTheme(ThemeA.Get(), Error));
	TestEqual(TEXT("Catalog failure retains the committed catalog"), InstanceA->GetCatalog(), Catalog.Get());
	TestEqual(TEXT("Another owner retains its active theme"), InstanceB->GetActiveTheme(), ThemeB.Get());
	InstanceA->Deinitialize();
	TestNull(TEXT("Deinitialize releases active theme"), InstanceA->GetActiveTheme());
	TestNull(TEXT("Deinitialize releases catalog"), InstanceA->GetCatalog());
	TestEqual(TEXT("Deinitialize does not clear another owner"), InstanceB->GetActiveTheme(), ThemeB.Get());
	Settings->DefaultTheme = SavedDefault;
	Settings->AvailableThemes = SavedAvailable;
	Settings->StyleCatalog = SavedCatalog;
	FWidgetThemeResolver::InvalidateEditorDefault();
	return true;
}
#endif
