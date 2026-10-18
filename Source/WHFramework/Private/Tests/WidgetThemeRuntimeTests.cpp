#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Tests/WidgetThemeTestTags.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Style/CommonButtonStyleN.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Theme/WidgetThemeSubsystem.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/WidgetModule.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
#include "Setting/Widget/Entry/WidgetNumberSettingEntryBase.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestButton, "UI.Style.Button.ThemeTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestText, "UI.Style.Text.ThemeTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ThemeTestBrush, "UI.Style.Brush.ThemeTest");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorParentA, "UI.Style.Brush.ThemeEditorParentA");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorParentB, "UI.Style.Brush.ThemeEditorParentB");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorChild, "UI.Style.Brush.ThemeEditorParentA.Child");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorAlias, "UI.Style.Brush.ThemeEditorAlias");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorButton, "UI.Style.Button.ThemeEditorButton");
UE_DEFINE_GAMEPLAY_TAG(TAG_ThemeEditorText, "UI.Style.Text.ThemeEditorText");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetThemeProjectDefaultOnlyTest, "WHFramework.WidgetTheme.ProjectDefaultOnly",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetThemeProjectDefaultOnlyTest::RunTest(const FString& Parameters)
{
	const FString ExpectedThemePath = TEXT("/Game/DataAssets/Theme/DA_WidgetTheme_Main.DA_WidgetTheme_Main");
	const UWidgetThemeSettings* Settings = GetDefault<UWidgetThemeSettings>();
	TestEqual(TEXT("Project settings own the single default theme path"), Settings->DefaultTheme.ToSoftObjectPath().ToString(), ExpectedThemePath);
	TestEqual(TEXT("Project settings expose exactly one theme"), Settings->AvailableThemes.Num(), 1);
	if (Settings->AvailableThemes.Num() == 1)
	{
		TestEqual(TEXT("The only available theme is the project Main theme"), Settings->AvailableThemes[0].ToSoftObjectPath().ToString(), ExpectedThemePath);
	}
	const UWidgetTheme* ResolvedTheme = FWidgetThemeResolver::ResolveTheme(UWidgetModule::StaticClass()->GetDefaultObject());
	const UWidgetTheme* ConfiguredTheme = Settings->DefaultTheme.LoadSynchronous();
	TestNotNull(TEXT("Editor theme resolution loads from project settings"), ResolvedTheme);
	TestEqual(TEXT("Editor resolution returns the configured Main theme"), ResolvedTheme, ConfiguredTheme);
	TestNull(TEXT("WidgetModule no longer stores a per-module theme property"), UWidgetModule::StaticClass()->FindPropertyByName(FName(TEXT("DefaultWidgetTheme"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetThemeTypedMapTest, "WHFramework.WidgetTheme.TypedMapAndValidation",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetThemeTypedMapTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWidgetTheme> Theme(NewObject<UWidgetTheme>());
	FString Error;
	TestTrue(TEXT("Partial theme is valid with configured fallback bases"), FWidgetThemeResolver::Validate(Theme.Get(), Error));
	TestFalse(TEXT("Text class cannot enter button map"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Button, UCommonTextStyle::StaticClass()));
	TestTrue(TEXT("Button class accepted"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Button, UCommonButtonStyleN::StaticClass()));
	TestFalse(TEXT("Root determines type"), Theme->SetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Text, UCommonTextStyle::StaticClass()));
	TestTrue(TEXT("Brush class accepted"), Theme->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UCommonBrushStyle::StaticClass()));
	TestTrue(TEXT("Configured mappings validate"), FWidgetThemeResolver::Validate(Theme.Get(), Error));
	TestNull(TEXT("No cross-type map lookup"), Theme->GetStyleClass(TAG_ThemeTestButton, EWidgetStyleKind::Text));
	Theme->RemoveStyle(TAG_ThemeTestBrush, EWidgetStyleKind::Brush);
	TestTrue(TEXT("Missing mapping does not invalidate a theme"), FWidgetThemeResolver::Validate(Theme.Get(), Error));
	for (EWidgetStyleKind Kind : WidgetStyleTags::Kinds())
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
	ThemeA->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UCommonBrushStyle::StaticClass());
	ThemeB->SetStyleClass(TAG_ThemeTestBrush, EWidgetStyleKind::Brush, UCommonBrushStyle::StaticClass());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetThemeSettingRendererInheritanceTest, "WHFramework.WidgetTheme.SettingRendererInheritance",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetThemeSettingRendererInheritanceTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWidgetTheme> Framework(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UWidgetTheme> Utilities(NewObject<UWidgetTheme>());
	TStrongObjectPtr<UWidgetTheme> Game(NewObject<UWidgetTheme>());
	Utilities->ParentTheme = Framework.Get();
	Game->ParentTheme = Utilities.Get();

	FSettingRendererClasses FrameworkRenderers;
	FrameworkRenderers.BoolClass = UWidgetSettingEntryBase::StaticClass();
	FrameworkRenderers.NumberClass = UWidgetSettingEntryBase::StaticClass();
	FrameworkRenderers.EnumClass = UWidgetSettingEntryBase::StaticClass();
	FrameworkRenderers.TextClass = UWidgetSettingEntryBase::StaticClass();
	FrameworkRenderers.OptionClass = UWidgetSettingEntryBase::StaticClass();
	FrameworkRenderers.KeyClass = UWidgetSettingEntryBase::StaticClass();
	FrameworkRenderers.CustomClass = UWidgetSettingEntryBase::StaticClass();
	Framework->SetSettingRendererClasses(FrameworkRenderers);

	FSettingRendererClasses GameRenderers;
	GameRenderers.NumberClass = UWidgetNumberSettingEntryBase::StaticClass();
	Game->SetSettingRendererClasses(GameRenderers);

	TestEqual(TEXT("Game resolves inherited Bool renderer through Utilities to Framework"),
		Game->FindSettingRendererClass(ESettingRendererType::Bool).Get(), UWidgetSettingEntryBase::StaticClass());
	TestEqual(TEXT("Game resolves inherited Text renderer through Utilities to Framework"),
		Game->FindSettingRendererClass(ESettingRendererType::Text).Get(), UWidgetSettingEntryBase::StaticClass());
	TestEqual(TEXT("Game resolves inherited Enum renderer through Utilities to Framework"),
		Game->FindSettingRendererClass(ESettingRendererType::Enum).Get(), UWidgetSettingEntryBase::StaticClass());
	TestEqual(TEXT("Game resolves inherited Option renderer through Utilities to Framework"),
		Game->FindSettingRendererClass(ESettingRendererType::Option).Get(), UWidgetSettingEntryBase::StaticClass());
	TestEqual(TEXT("Game resolves inherited Key renderer through Utilities to Framework"),
		Game->FindSettingRendererClass(ESettingRendererType::Key).Get(), UWidgetSettingEntryBase::StaticClass());
	TestEqual(TEXT("Game resolves inherited Custom renderer through Utilities to Framework"),
		Game->FindSettingRendererClass(ESettingRendererType::Custom).Get(), UWidgetSettingEntryBase::StaticClass());
	TestEqual(TEXT("Game local Number renderer takes precedence"),
		Game->FindSettingRendererClass(ESettingRendererType::Number).Get(), UWidgetNumberSettingEntryBase::StaticClass());
	return true;
}
#endif
