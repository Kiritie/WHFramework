#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Tests/WidgetThemeTestTags.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Style/SubWidgetStyle.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Theme/WidgetThemeSubsystem.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestButton, "Style.Button.ThemeTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestText, "Style.Text.ThemeTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestBrush, "Style.Brush.ThemeTest");
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
	FString Error;
	TestTrue(TEXT("Partial theme is valid with configured fallback bases"), FWidgetThemeResolver::Validate(Theme.Get(), Error));
	TestFalse(TEXT("Text class cannot enter button map"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Button, UCommonTextStyle::StaticClass()));
	TestTrue(TEXT("Button class accepted"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Button, USubWidgetStyle::StaticClass()));
	TestFalse(TEXT("Root determines type"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Text, UCommonTextStyle::StaticClass()));
	TestTrue(TEXT("Brush class accepted"), Theme->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass()));
	TestTrue(TEXT("Configured mappings validate"), FWidgetThemeResolver::Validate(Theme.Get(), Error));
	TestNull(TEXT("No cross-type map lookup"), Theme->GetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Text));
	Theme->RemoveStyle(TAG_ThemeTestBrush, EWidgetStyleKind::Brush);
	TestTrue(TEXT("Missing mapping does not invalidate a theme"), FWidgetThemeResolver::Validate(Theme.Get(), Error));
	for (EWidgetStyleKind Kind : {EWidgetStyleKind::Button, EWidgetStyleKind::Text, EWidgetStyleKind::Brush})
	{
		TestEqual(TEXT("Runtime fallback and authoring use the configured base"), FWidgetThemeResolver::DefaultStyle(Kind),
				  GetDefault<UWidgetThemeSettings>()->GetStyleBase(Kind));
	}
	EWidgetStyleKind Kind;
	TestTrue(TEXT("Button root classifies descendants"), WidgetStyleTags::Kind(TAG_ThemeTestButton, Kind) && Kind == EWidgetStyleKind::Button);
	TestTrue(TEXT("Text root classifies descendants"), WidgetStyleTags::Kind(TAG_ThemeTestText, Kind) && Kind == EWidgetStyleKind::Text);
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
	TStrongObjectPtr<UWidgetTheme> ThemeA(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UWidgetTheme> ThemeB(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UGameInstance> OwnerA(NewObject<UGameInstance>());
	TStrongObjectPtr<UGameInstance> OwnerB(NewObject<UGameInstance>());
	TStrongObjectPtr<UWidgetThemeSubsystem> InstanceA(NewObject<UWidgetThemeSubsystem>(OwnerA.Get()));
	TStrongObjectPtr<UWidgetThemeSubsystem> InstanceB(NewObject<UWidgetThemeSubsystem>(OwnerB.Get()));
	ThemeA->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass());
	ThemeB->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UWidgetBrushStyle::StaticClass());
	Settings->DefaultTheme = ThemeA.Get();
	Settings->AvailableThemes = {ThemeA.Get(), ThemeB.Get()};
	FString Error;
	TestTrue(TEXT("Instance A commits A"), InstanceA->ApplyLoadedTheme(ThemeA.Get(), Error));
	TestTrue(TEXT("Instance B commits B"), InstanceB->ApplyLoadedTheme(ThemeB.Get(), Error));
	const uint64 PreviousSerial = InstanceA->RequestSerial;
	TestTrue(TEXT("New switch commits B"), InstanceA->ApplyLoadedTheme(ThemeB.Get(), Error));
	InstanceA->CompleteRequest(PreviousSerial, ThemeA.Get());
	TestEqual(TEXT("Stale completion cannot replace the latest theme"), InstanceA->GetActiveTheme(), ThemeB.Get());
	TestFalse(TEXT("Missing theme is rejected"), InstanceA->ApplyLoadedTheme(nullptr, Error));
	TestEqual(TEXT("Rejected request retains previous active theme"), InstanceA->GetActiveTheme(), ThemeB.Get());
	ThemeA->bRuntimeTheme = false;
	TestFalse(TEXT("Library theme is rejected"), InstanceA->ApplyLoadedTheme(ThemeA.Get(), Error));
	TestEqual(TEXT("Rejected library retains the committed theme"), InstanceA->GetActiveTheme(), ThemeB.Get());
	TestEqual(TEXT("Another owner retains its active theme"), InstanceB->GetActiveTheme(), ThemeB.Get());
	InstanceA->Deinitialize();
	TestNull(TEXT("Deinitialize releases active theme"), InstanceA->GetActiveTheme());
	TestEqual(TEXT("Deinitialize does not clear another owner"), InstanceB->GetActiveTheme(), ThemeB.Get());
	Settings->DefaultTheme = SavedDefault;
	Settings->AvailableThemes = SavedAvailable;
	FWidgetThemeResolver::InvalidateEditorDefault();
	return true;
}
#endif
