#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WidgetThemeTestTags.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetThemeFactory.h"
#include "Widget/Theme/WidgetStyleTags.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Theme/WidgetThemeEditorSettings.h"
#include "Widget/Style/WidgetBrushStyle.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintEditorLibrary.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/NamePermissionList.h"
#include "Misc/PackageName.h"
#include "Editor.h"
#include "ObjectTools.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"
#include "Widget/Theme/WidgetThemeToolkit.h"
#include "Widget/Theme/WidgetThemeTree.h"
#include "Widget/Theme/WidgetThemePreviewButton.h"
#include "Widget/Common/CommonTextBlockN.h"
#include "Widget/Common/CommonImageN.h"
#include "Widget/Theme/WidgetStylePropertyEditing.h"
#include "Widget/Theme/WidgetStyleClipboard.h"
#include "UObject/Linker.h"
#include "UObject/ObjectRedirector.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"

namespace WidgetThemeEditorTests
{
	struct FFixture
	{
		TArray<TWeakObjectPtr<UWidgetTheme>> Themes;
		TArray<TWeakObjectPtr<UObjectRedirector>> Redirectors;
		FString Directory;
		FString Clipboard;
		TArray<uint8> TagConfig;
		TArray<FString> TextFiles;
		FFixture()
		{
			Directory = TEXT("/Game/WidgetThemeTests/") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			WidgetStyleClipboard::Read(Clipboard);
			FFileHelper::LoadFileToArray(TagConfig, *GetDefault<UGameplayTagsSettings>()->GetDefaultConfigFilename());
		}
		~FFixture()
		{
			TArray<UObject*> Objects;
			for (const auto& Redirector : Redirectors)
			{
				if (Redirector.IsValid())
					Objects.Add(Redirector.Get());
			}
			for (const auto& Theme : Themes)
			{
				if (!Theme.IsValid())
				{
					continue;
				}
				for (const auto& Entry : Theme->Bindings)
				{
					if (Entry.Value.bManaged)
					{
						Objects.AddUnique(FWidgetThemeEditorService::Blueprint(Theme.Get(), Entry.Key));
					}
				}
				Objects.AddUnique(Theme.Get());
			}
			for (UObject* Object : Objects)
			{
				if (!IsValid(Object))
				{
					continue;
				}
				const FString Filename = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
				ResetLoaders(Object->GetOutermost());
				FAssetRegistryModule::AssetDeleted(Object);
				Object->ClearFlags(RF_Public | RF_Standalone);
				Object->GetOutermost()->SetDirtyFlag(false);
				const FString Name = TEXT("ThemeTestDiscard_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
				Object->Rename(*Name, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				if (IFileManager::Get().FileExists(*Filename))
				{
					IFileManager::Get().Delete(*Filename);
				}
			}
			for (const FString& File : TextFiles)
			{
				IFileManager::Get().Delete(*File);
			}
			FFileHelper::SaveArrayToFile(TagConfig, *GetDefault<UGameplayTagsSettings>()->GetDefaultConfigFilename());
			GConfig->UnloadFile(GetDefault<UGameplayTagsSettings>()->GetDefaultConfigFilename());
			GConfig->LoadFile(GetDefault<UGameplayTagsSettings>()->GetDefaultConfigFilename());
			GetMutableDefault<UGameplayTagsSettings>()->ReloadConfig();
			UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
			WidgetStyleClipboard::Write(Clipboard);
			FWidgetThemeResolver::InvalidateEditorDefault();
		}
		UWidgetTheme* Theme(const FString& Name)
		{
			UWidgetTheme* Result = NewObject<UWidgetTheme>(CreatePackage(*(Directory / Name)), FName(*Name), RF_Public | RF_Standalone | RF_Transactional);
			Result->GetOutermost()->MarkAsFullyLoaded();
			Result->bRuntimeTheme = false;
			FAssetRegistryModule::AssetCreated(Result);
			Themes.Add(Result);
			return Result;
		}
		UWidgetTheme* Duplicate(UWidgetTheme* Source)
		{
			UWidgetTheme* Result =
				Cast<UWidgetTheme>(FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().DuplicateAsset(TEXT("DA_Copy"), Directory, Source));
			Themes.Add(Result);
			return Result;
		}
		UObjectRedirector* Redirect(const FString& Name, UObject* Destination)
		{
			UObjectRedirector* Result = NewObject<UObjectRedirector>(CreatePackage(*(Directory / Name)), FName(*Name), RF_Public | RF_Standalone);
			Result->DestinationObject = Destination;
			FAssetRegistryModule::AssetCreated(Result);
			Redirectors.Add(Result);
			return Result;
		}
		bool Parent(UWidgetTheme* Theme, FString& Error)
		{
			return FWidgetThemeEditorService::Create(Theme, WidgetStyleTags::Brush, TEXT("ThemeEditorParentA"), EWidgetStyleKind::Brush, TEXT("/Game/Themes"),
													 false, Error);
		}
		UWidgetBrushStyle* Brush(UWidgetTheme* Theme, FGameplayTag Tag = TAG_ThemeEditorParentA)
		{
			UBlueprint* BP = FWidgetThemeEditorService::Blueprint(Theme, Tag);
			return BP ? BP->GeneratedClass->GetDefaultObject<UWidgetBrushStyle>() : nullptr;
		}
		bool Tint(UWidgetTheme* Theme, FGameplayTag Tag, FLinearColor Color, FString& Error)
		{
			UWidgetBrushStyle* Defaults = Brush(Theme, Tag);
			FWidgetStylePropertyEditing Properties(Defaults);
			FProperty* Property = Defaults->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UWidgetBrushStyle, Brush));
			TSharedPtr<IPropertyHandle> Handle = Properties.Find(Property);
			FSlateBrush Value = Defaults->Brush;
			Value.TintColor = FSlateColor(Color);
			FString Text;
			Property->ExportText_Direct(Text, &Value, nullptr, Defaults, PPF_None);
			if (!Handle || Handle->SetValueFromFormattedString(Text) != FPropertyAccess::Success)
			{
				Error = TEXT("Native property editor could not set Brush.");
				return false;
			}
			FBlueprintEditorUtils::MarkBlueprintAsModified(FWidgetThemeEditorService::Blueprint(Theme, Tag));
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeNativeInheritanceTest, "WHFramework.WidgetTheme.Editor.NativeInheritance",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeNativeInheritanceTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_Inheritance"));
	if (!TestTrue(*Error, Fixture.Parent(Theme, Error)) || !TestTrue(*Error, Fixture.Tint(Theme, TAG_ThemeEditorParentA, FLinearColor::Red, Error)))
	{
		return false;
	}
	if (!TestTrue(*Error,
				  FWidgetThemeEditorService::Create(Theme, TAG_ThemeEditorParentA, TEXT("Child"), EWidgetStyleKind::Brush, TEXT("/Game/Themes"), true, Error)))
	{
		return false;
	}
	UBlueprint* Child = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorChild);
	TestEqual(TEXT("Actual Blueprint parent"), Child->ParentClass.Get(), Theme->GetStyleClass(TAG_ThemeEditorParentA, EWidgetStyleKind::Brush));
	TestEqual(TEXT("Initial inherited value"), Fixture.Brush(Theme, TAG_ThemeEditorChild)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Red);
	Fixture.Tint(Theme, TAG_ThemeEditorParentA, FLinearColor::Green, Error);
	FKismetEditorUtilities::CompileBlueprint(Child);
	TestEqual(TEXT("Native inheritance follows parent"), Child->GeneratedClass->GetDefaultObject<UWidgetBrushStyle>()->Brush.TintColor.GetSpecifiedColor(),
			  FLinearColor::Green);
	Fixture.Tint(Theme, TAG_ThemeEditorChild, FLinearColor::Blue, Error);
	Fixture.Tint(Theme, TAG_ThemeEditorParentA, FLinearColor::Red, Error);
	FKismetEditorUtilities::CompileBlueprint(Child);
	TestEqual(TEXT("Native child override remains"), Child->GeneratedClass->GetDefaultObject<UWidgetBrushStyle>()->Brush.TintColor.GetSpecifiedColor(),
			  FLinearColor::Blue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeDuplicationTest, "WHFramework.WidgetTheme.Editor.ContentBrowserDuplication",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeDuplicationTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Source = Fixture.Theme(TEXT("DA_Source"));
	if (!Fixture.Parent(Source, Error) || !Fixture.Tint(Source, TAG_ThemeEditorParentA, FLinearColor::Red, Error) ||
		!FWidgetThemeEditorService::Create(Source, TAG_ThemeEditorParentA, TEXT("Child"), EWidgetStyleKind::Brush, TEXT("/Game/Themes"), true, Error))
	{
		AddError(Error);
		return false;
	}
	UWidgetTheme* Copy = Fixture.Duplicate(Source);
	if (!TestNotNull(TEXT("AssetTools native duplication"), Copy))
	{
		return false;
	}
	TestFalse(TEXT("Duplication completed"), Copy->bDuplicateFailed);
	UBlueprint* OriginalParent = FWidgetThemeEditorService::Blueprint(Source, TAG_ThemeEditorParentA);
	UBlueprint* NewParent = FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorParentA);
	UBlueprint* NewChild = FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorChild);
	TestNotEqual(TEXT("Owned parent is independent"), NewParent, OriginalParent);
	TestEqual(TEXT("Child points to cloned parent"), NewChild->ParentClass, NewParent->GeneratedClass);
	TestEqual(TEXT("Copied values preserved"), Fixture.Brush(Copy)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Red);
	Fixture.Tint(Copy, TAG_ThemeEditorParentA, FLinearColor::Blue, Error);
	TestEqual(TEXT("Editing copy leaves original unchanged"), Fixture.Brush(Source)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Red);
	TestTrue(TEXT("Copy has exactly one owner"), FWidgetThemeEditorService::Validate(Copy, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeSharedDuplicateTest, "WHFramework.WidgetTheme.Editor.SharedDuplication",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeSharedDuplicateTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Owner = Fixture.Theme(TEXT("DA_Owner"));
	UWidgetTheme* Borrower = Fixture.Theme(TEXT("DA_Borrower"));
	if (!Fixture.Parent(Owner, Error) ||
		!FWidgetThemeEditorService::Associate(Borrower, WidgetStyleTags::Brush, TEXT("ThemeEditorAlias"), Owner, TAG_ThemeEditorParentA, Error))
	{
		AddError(Error);
		return false;
	}
	UWidgetTheme* Copy = Fixture.Duplicate(Borrower);
	if (!TestNotNull(TEXT("Shared theme copied"), Copy))
	{
		return false;
	}
	TestEqual(TEXT("Shared class stays identical"), FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorAlias),
			  FWidgetThemeEditorService::Blueprint(Owner, TAG_ThemeEditorParentA));
	TestFalse(TEXT("Shared mapping stays readonly"), Copy->Bindings[TAG_ThemeEditorAlias].bManaged);
	TestEqual(TEXT("Source owner preserved"), Copy->Bindings[TAG_ThemeEditorAlias].SourceTheme.Get(), Owner);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeAliasDuplicateTest, "WHFramework.WidgetTheme.Editor.AliasDuplication",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeAliasDuplicateTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Source = Fixture.Theme(TEXT("DA_Aliases"));
	if (!Fixture.Parent(Source, Error) ||
		!FWidgetThemeEditorService::Associate(Source, WidgetStyleTags::Brush, TEXT("ThemeEditorAlias"), Source, TAG_ThemeEditorParentA, Error))
	{
		AddError(Error);
		return false;
	}
	UWidgetTheme* Copy = Fixture.Duplicate(Source);
	if (!TestNotNull(TEXT("Theme copied"), Copy))
	{
		return false;
	}
	TestEqual(TEXT("One private clone for two tags"), FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorParentA),
			  FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorAlias));
	TestEqual(TEXT("Alias owner remapped"), Copy->Bindings[TAG_ThemeEditorAlias].SourceTheme.Get(), Copy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeClipboardTest, "WHFramework.WidgetTheme.Editor.ClipboardProperties",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeClipboardTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Source = Fixture.Theme(TEXT("DA_ClipboardSource"));
	UWidgetTheme* Target = Fixture.Theme(TEXT("DA_ClipboardTarget"));
	if (!Fixture.Parent(Source, Error) || !Fixture.Parent(Target, Error))
	{
		AddError(Error);
		return false;
	}
	Fixture.Brush(Source)->Brush.ImageSize = FVector2D(91, 37);
	Fixture.Tint(Source, TAG_ThemeEditorParentA, FLinearColor::Blue, Error);
	UBlueprint* Original = FWidgetThemeEditorService::Blueprint(Target, TAG_ThemeEditorParentA);
	UClass* Parent = Original->ParentClass;
	if (!TestTrue(TEXT("Cross-editor clipboard copy"), FWidgetThemeEditorService::CopyProperties(Source, TAG_ThemeEditorParentA, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Same kind paste enabled"), FWidgetThemeEditorService::CanPasteProperties(Target, TAG_ThemeEditorParentA));
	TestTrue(TEXT("Paste values"), FWidgetThemeEditorService::PasteProperties(Target, TAG_ThemeEditorParentA, Error));
	TestEqual(TEXT("Complete brush size"), FVector2D(Fixture.Brush(Target)->Brush.ImageSize), FVector2D(91, 37));
	TestEqual(TEXT("Complete brush tint"), Fixture.Brush(Target)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Blue);
	TestEqual(TEXT("Asset identity retained"), FWidgetThemeEditorService::Blueprint(Target, TAG_ThemeEditorParentA), Original);
	TestEqual(TEXT("Parent retained"), Original->ParentClass.Get(), Parent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeClipboardGuardsTest, "WHFramework.WidgetTheme.Editor.ClipboardGuards",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeClipboardGuardsTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_ClipboardGuards"));
	UWidgetTheme* Shared = Fixture.Theme(TEXT("DA_Readonly"));
	if (!Fixture.Parent(Theme, Error) ||
		!FWidgetThemeEditorService::Create(Theme, WidgetStyleTags::Text, TEXT("ThemeEditorText"), EWidgetStyleKind::Text, TEXT("/Game/Themes"), false, Error) ||
		!FWidgetThemeEditorService::Associate(Shared, WidgetStyleTags::Brush, TEXT("ThemeEditorAlias"), Theme, TAG_ThemeEditorParentA, Error))
	{
		AddError(Error);
		return false;
	}
	FPlatformApplicationMisc::ClipboardCopy(TEXT("ordinary text"));
	TestFalse(TEXT("Unrelated clipboard disabled"), FWidgetThemeEditorService::CanPasteProperties(Theme, TAG_ThemeEditorParentA));
	if (!FWidgetThemeEditorService::CopyProperties(Theme, TAG_ThemeEditorParentA, Error))
	{
		AddError(Error);
		return false;
	}
	TestFalse(TEXT("Cross-kind paste disabled"), FWidgetThemeEditorService::CanPasteProperties(Theme, TAG_ThemeEditorText));
	TestFalse(TEXT("Shared paste disabled"), FWidgetThemeEditorService::CanPasteProperties(Shared, TAG_ThemeEditorAlias));
	TestFalse(TEXT("Root paste disabled"), FWidgetThemeEditorService::CanPasteProperties(Theme, WidgetStyleTags::Brush));
	TestTrue(TEXT("Shared copy allowed"), FWidgetThemeEditorService::CopyProperties(Shared, TAG_ThemeEditorAlias, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemePasteUndoTest, "WHFramework.WidgetTheme.Editor.PasteUndoRedo",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemePasteUndoTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Source = Fixture.Theme(TEXT("DA_UndoSource"));
	UWidgetTheme* Target = Fixture.Theme(TEXT("DA_UndoTarget"));
	if (!Fixture.Parent(Source, Error) || !Fixture.Parent(Target, Error))
	{
		return false;
	}
	Fixture.Tint(Source, TAG_ThemeEditorParentA, FLinearColor::Red, Error);
	Fixture.Tint(Target, TAG_ThemeEditorParentA, FLinearColor::Blue, Error);
	if (!FWidgetThemeEditorService::CopyProperties(Source, TAG_ThemeEditorParentA, Error) ||
		!FWidgetThemeEditorService::PasteProperties(Target, TAG_ThemeEditorParentA, Error))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Pasted red"), Fixture.Brush(Target)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Red);
	GEditor->UndoTransaction();
	TestEqual(TEXT("Undo restored blue"), Fixture.Brush(Target)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Blue);
	GEditor->RedoTransaction();
	TestEqual(TEXT("Redo restored red"), Fixture.Brush(Target)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Red);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeNameTest, "WHFramework.WidgetTheme.Editor.SingleSegmentNames",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeNameTest::RunTest(const FString& Parameters)
{
	FString Error;
	for (const FString& Name : {TEXT(""), TEXT("More.Levels"), TEXT(" Name"), TEXT("Name "), TEXT("A/B"), TEXT("A B")})
	{
		TestFalse(*Name, FWidgetThemeEditorService::ValidateName(Name, Error));
	}
	TestTrue(TEXT("Valid leaf"), FWidgetThemeEditorService::ValidateName(TEXT("Button_01"), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeRootTest, "WHFramework.WidgetTheme.Editor.RootsAndDestinations",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeRootTest::RunTest(const FString& Parameters)
{
	for (EWidgetStyleKind Type : {EWidgetStyleKind::Button, EWidgetStyleKind::Text, EWidgetStyleKind::Brush})
	{
		EWidgetStyleKind Actual;
		TestTrue(TEXT("Static root registered"), WidgetStyleTags::Root(Type).IsValid());
		TestTrue(TEXT("Root recognized"), WidgetStyleTags::Kind(WidgetStyleTags::Root(Type), Actual));
		TestEqual(TEXT("Root kind"), Actual, Type);
	}
	const TArray<FString> Roots = FWidgetThemeEditorService::GetCreationRoots();
	TestEqual(TEXT("Game first"), Roots[0], FString(TEXT("/Game/Themes")));
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Engine)
		{
			TestFalse(*Plugin->GetName(), Roots.Contains(Plugin->GetMountedAssetPath() / TEXT("Themes")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeDeleteProtectionTest, "WHFramework.WidgetTheme.Editor.DeleteReferenceProtection",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeDeleteProtectionTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Owner = Fixture.Theme(TEXT("DA_DeleteOwner"));
	UWidgetTheme* Borrower = Fixture.Theme(TEXT("DA_DeleteBorrower"));
	if (!Fixture.Parent(Owner, Error) ||
		!FWidgetThemeEditorService::Associate(Borrower, WidgetStyleTags::Brush, TEXT("ThemeEditorAlias"), Owner, TAG_ThemeEditorParentA, Error))
	{
		return false;
	}
	TestFalse(TEXT("External shared reference blocks cascading delete"), FWidgetThemeEditorService::CanDeleteAssets({Owner}, Error));
	TestFalse(TEXT("Root cannot be removed"), FWidgetThemeEditorService::CanRemove(Owner, WidgetStyleTags::Brush, Error));
	TestTrue(TEXT("Shared mapping can be removed"), FWidgetThemeEditorService::RemoveBinding(Borrower, TAG_ThemeEditorAlias, Error));
	TestNotNull(TEXT("Shared owner Blueprint retained"), FWidgetThemeEditorService::Blueprint(Owner, TAG_ThemeEditorParentA));
	TestTrue(TEXT("Owner can be deleted after borrower removal"), FWidgetThemeEditorService::CanDeleteAssets({Owner}, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeOwnedRemoveTest, "WHFramework.WidgetTheme.Editor.OwnedRemovalInternalReferences",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeOwnedRemoveTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_InternalReferences"));
	if (!FWidgetThemeEditorService::Create(Theme, WidgetStyleTags::Text, TEXT("ThemeEditorText"), EWidgetStyleKind::Text, TEXT("/Game/Themes"), false, Error) ||
		!FWidgetThemeEditorService::Create(Theme, WidgetStyleTags::Button, TEXT("ThemeEditorButton"), EWidgetStyleKind::Button, TEXT("/Game/Themes"), false,
										   Error))
	{
		AddError(Error);
		return false;
	}
	UBlueprint* TextStyle = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorText);
	const FSoftObjectPath StylePath(TextStyle);
	UBlueprint* ButtonStyle = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorButton);
	FObjectPropertyBase* TextProperty = FindFProperty<FObjectPropertyBase>(ButtonStyle->GeneratedClass, TEXT("NormalTextStyle"));
	if (!TestNotNull(TEXT("Button text style property"), TextProperty))
		return false;
	TextProperty->SetObjectPropertyValue_InContainer(ButtonStyle->GeneratedClass->GetDefaultObject(), TextStyle->GeneratedClass);
	UObjectRedirector* Redirector = Fixture.Redirect(TEXT("OldTextStyle"), TextStyle);
	const FSoftObjectPath RedirectorPath(Redirector);
	TestTrue(TEXT("Owned style with internal button reference and redirector can be removed"),
			 FWidgetThemeEditorService::CanRemove(Theme, TAG_ThemeEditorText, Error));
	TestTrue(TEXT("Owned text removal succeeds"), FWidgetThemeEditorService::RemoveBinding(Theme, TAG_ThemeEditorText, Error));
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TestFalse(TEXT("Owned text Blueprint deleted"), Registry.GetAssetByObjectPath(StylePath, true).IsValid());
	TestFalse(TEXT("Old text redirector deleted"), Registry.GetAssetByObjectPath(RedirectorPath, true).IsValid());
	ButtonStyle = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorButton);
	if (!TestNotNull(TEXT("Referencing button retained"), ButtonStyle))
		return false;
	TextProperty = FindFProperty<FObjectPropertyBase>(ButtonStyle->GeneratedClass, TEXT("NormalTextStyle"));
	if (!TestNotNull(TEXT("Button text property retained"), TextProperty))
		return false;
	TestNull(TEXT("Deleted text class reference cleared"), TextProperty->GetObjectPropertyValue_InContainer(ButtonStyle->GeneratedClass->GetDefaultObject()));
	if (!Fixture.Parent(Theme, Error) ||
		!FWidgetThemeEditorService::Create(Theme, TAG_ThemeEditorParentA, TEXT("Child"), EWidgetStyleKind::Brush, TEXT("/Game/Themes"), true, Error))
		return false;
	UClass* OriginalParent = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorParentA)->ParentClass;
	TestTrue(TEXT("Owned parent with child can be removed"), FWidgetThemeEditorService::CanRemove(Theme, TAG_ThemeEditorParentA, Error));
	TestTrue(TEXT("Owned parent removal succeeds"), FWidgetThemeEditorService::RemoveBinding(Theme, TAG_ThemeEditorParentA, Error));
	UBlueprint* Child = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorChild);
	if (TestNotNull(TEXT("Child retained"), Child))
		TestEqual(TEXT("Child inherits deleted parent's parent"), Child->ParentClass.Get(), OriginalParent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeReferencedTagRemoveTest, "WHFramework.WidgetTheme.Editor.OwnedRemovalPreservesReferencedTag",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeReferencedTagRemoveTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Owner = Fixture.Theme(TEXT("DA_TagOwner"));
	UWidgetTheme* Consumer = Fixture.Theme(TEXT("DA_TagConsumer"));
	const FString Name = TEXT("TestReferenced_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!FWidgetThemeEditorService::Create(Owner, WidgetStyleTags::Brush, Name, EWidgetStyleKind::Brush, TEXT("/Game/Themes"), false, Error))
		return false;
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*(TEXT("Style.Brush.") + Name)));
	const FSoftObjectPath StylePath(FWidgetThemeEditorService::Blueprint(Owner, Tag));
	Consumer->SetStyleClass(Tag, EWidgetStyleKind::Brush, FWidgetThemeEditorService::CreationBase(EWidgetStyleKind::Brush));
	TestTrue(TEXT("Another asset's tag usage does not disable removal"), FWidgetThemeEditorService::CanRemove(Owner, Tag, Error));
	TestTrue(TEXT("Referenced-tag style removed"), FWidgetThemeEditorService::RemoveBinding(Owner, Tag, Error));
	TestFalse(TEXT("Owned Blueprint deleted"),
			  FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetByObjectPath(StylePath, true).IsValid());
	TestTrue(TEXT("Referenced ordinary tag retained"), FGameplayTag::RequestGameplayTag(Tag.GetTagName(), false).IsValid());
	TestNotNull(TEXT("Other asset's mapping retained"), Consumer->GetStyleClass(Tag, EWidgetStyleKind::Brush));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeNativeDeleteTest, "WHFramework.WidgetTheme.Editor.ContentBrowserDeletion",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeNativeDeleteTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_Delete"));
	if (!Fixture.Parent(Theme, Error))
	{
		return false;
	}
	const FSoftObjectPath StylePath(FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorParentA));
	const FSoftObjectPath ThemePath(Theme);
	UObjectRedirector* ThemeRedirector = Fixture.Redirect(TEXT("DA_OldTheme"), Theme);
	UObjectRedirector* StyleRedirector = Fixture.Redirect(TEXT("OldStyle"), StylePath.ResolveObject());
	const FSoftObjectPath ThemeRedirectorPath(ThemeRedirector);
	const FSoftObjectPath StyleRedirectorPath(StyleRedirector);
	TArray<FString> SavedFiles;
	for (UObject* Asset : TArray<UObject*>{Theme, StylePath.ResolveObject(), ThemeRedirector, StyleRedirector})
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!TestTrue(TEXT("Save deletion test package"), UPackage::SavePackage(Package, nullptr, *Filename, Args)))
		{
			return false;
		}
		SavedFiles.Add(Filename);
	}
	struct FRestoreSettings
	{
		TSoftObjectPtr<UWidgetTheme> Default = GetDefault<UWidgetThemeSettings>()->DefaultTheme;
		TArray<TSoftObjectPtr<UWidgetTheme>> Available = GetDefault<UWidgetThemeSettings>()->AvailableThemes;
		TArray<uint8> Config;
		FRestoreSettings()
		{
			FFileHelper::LoadFileToArray(Config, *GetDefault<UWidgetThemeSettings>()->GetDefaultConfigFilename());
		}
		~FRestoreSettings()
		{
			UWidgetThemeSettings* Settings = GetMutableDefault<UWidgetThemeSettings>();
			Settings->DefaultTheme = Default;
			Settings->AvailableThemes = Available;
			FFileHelper::SaveArrayToFile(Config, *Settings->GetDefaultConfigFilename());
			GConfig->UnloadFile(Settings->GetDefaultConfigFilename());
			GConfig->LoadFile(Settings->GetDefaultConfigFilename());
			FWidgetThemeResolver::InvalidateEditorDefault();
		}
	} Restore;
	GetMutableDefault<UWidgetThemeSettings>()->DefaultTheme = Theme;
	GetMutableDefault<UWidgetThemeSettings>()->AvailableThemes.Add(Theme);
	FEditorDelegates::OnAssetsPreDelete.Broadcast({Theme, StylePath.ResolveObject()});
	TestTrue(TEXT("Pre-delete cancellation leaves theme intact"), Theme->IsAsset());
	TestTrue(TEXT("Pre-delete cancellation leaves owned style intact"), StylePath.ResolveObject()->IsAsset());
	TestTrue(TEXT("Deletion preflight passes"), FWidgetThemeEditorService::CanDeleteAssets({Theme}, Error));
	TestTrue(TEXT("Native deletion including owned Blueprint"), ObjectTools::DeleteObjects({Theme}, false) > 0);
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TestFalse(TEXT("Theme removed"), Registry.GetAssetByObjectPath(ThemePath, true).IsValid());
	TestFalse(TEXT("Owned Blueprint removed"), Registry.GetAssetByObjectPath(StylePath, true).IsValid());
	TestFalse(TEXT("Old theme redirector removed"), Registry.GetAssetByObjectPath(ThemeRedirectorPath, true).IsValid());
	TestFalse(TEXT("Old style redirector removed"), Registry.GetAssetByObjectPath(StyleRedirectorPath, true).IsValid());
	for (const FString& Filename : SavedFiles)
	{
		TestFalse(TEXT("Deleted package absent from disk"), IFileManager::Get().FileExists(*Filename));
	}
	TestTrue(TEXT("Theme no longer an asset"), !ThemePath.ResolveObject() || !ThemePath.ResolveObject()->IsAsset());
	TestTrue(TEXT("Style no longer an asset"), !StylePath.ResolveObject() || !StylePath.ResolveObject()->IsAsset());
	TestTrue(TEXT("Deleted default theme cleared"), GetDefault<UWidgetThemeSettings>()->DefaultTheme.IsNull());
	TestFalse(TEXT("Deleted theme removed from available list"),
			  GetDefault<UWidgetThemeSettings>()->AvailableThemes.Contains(TSoftObjectPtr<UWidgetTheme>(ThemePath)));
	FString SavedConfig;
	FFileHelper::LoadFileToString(SavedConfig, *GetDefault<UWidgetThemeSettings>()->GetDefaultConfigFilename());
	TestFalse(TEXT("Deleted theme config path persisted as removed"), SavedConfig.Contains(ThemePath.ToString()));
	TestTrue(TEXT("Static roots retained"), WidgetStyleTags::Brush.GetTag().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeFactoryTest, "WHFramework.WidgetTheme.Editor.ContentBrowserFactory",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeFactoryTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	UWidgetThemeFactory* Factory = NewObject<UWidgetThemeFactory>();
	UWidgetTheme* Theme = Cast<UWidgetTheme>(Factory->FactoryCreateNew(UWidgetTheme::StaticClass(), CreatePackage(*(Fixture.Directory / TEXT("DA_Readable"))),
																	   TEXT("DA_Readable"), RF_Public | RF_Standalone, nullptr, GWarn));
	Fixture.Themes.Add(Theme);
	TestNotNull(TEXT("No creation panel"), Theme);
	TestEqual(TEXT("Readable user name preserved"), Theme->GetName(), FString(TEXT("DA_Readable")));
	TestTrue(TEXT("Arbitrary Content Browser folder preserved"), Theme->GetOutermost()->GetName().StartsWith(Fixture.Directory));
	TestFalse(TEXT("New theme is a library until set default"), Theme->bRuntimeTheme);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeFolderTest, "WHFramework.WidgetTheme.Editor.FolderVisibility",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeFolderTest::RunTest(const FString& Parameters)
{
	UWidgetThemeEditorSettings::ApplyFolderVisibility();
	auto Visibility = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().GetFolderPermissionList();
	if (GetDefault<UWidgetThemeEditorSettings>()->bHideThemeFolders)
	{
		TestFalse(TEXT("Style folder hidden"), Visibility->PassesStartsWithFilter(TEXT("/Game/Themes")));
		TestTrue(TEXT("Theme asset folder visible"), Visibility->PassesStartsWithFilter(TEXT("/Game/DataAssets/Theme")));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeTreeAndLayoutTest, "WHFramework.WidgetTheme.Editor.TreeAndDockLayout",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeTreeAndLayoutTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Owner = Fixture.Theme(TEXT("DA_TreeOwner"));
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_Tree"));
	if (!Fixture.Parent(Owner, Error) ||
		!FWidgetThemeEditorService::Associate(Theme, WidgetStyleTags::Brush, TEXT("ThemeEditorParentA"), Owner, TAG_ThemeEditorParentA, Error) ||
		!FWidgetThemeEditorService::Create(Theme, TAG_ThemeEditorParentA, TEXT("Child"), EWidgetStyleKind::Brush, TEXT("/Game/Themes"), true, Error))
	{
		AddError(Error);
		return false;
	}
	const auto Roots = FWidgetThemeTree::Build(Theme, EWidgetStyleKind::None, TEXT(""));
	TestEqual(TEXT("Three roots always present"), Roots.Num(), 3);
	TestTrue(TEXT("No outer Style node"), Roots[0]->bRoot && Roots[0]->Tag == WidgetStyleTags::Button);
	const auto BrushRoot = Roots[2];
	TestEqual(TEXT("Shared parent visible"), BrushRoot->Children.Num(), 1);
	if (BrushRoot->Children.Num() == 1)
	{
		TestTrue(TEXT("Shared marker"), BrushRoot->Children[0]->bShared);
		TestEqual(TEXT("Child mounted under actual shared parent"), BrushRoot->Children[0]->Children.Num(), 1);
	}
	const FString Layout = FWidgetThemeToolkit::CreateDefaultLayout()->ToString();
	for (const TCHAR* Tab : {TEXT("WidgetTheme.Tags"), TEXT("WidgetTheme.Preview"), TEXT("WidgetTheme.Details"), TEXT("WidgetTheme.Context")})
	{
		TestTrue(Tab, Layout.Contains(Tab));
	}
	TestEqual(TEXT("Type filter retains corresponding root"), FWidgetThemeTree::Build(Theme, EWidgetStyleKind::Text, TEXT("")).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeExistingUIPreviewTest, "WHFramework.WidgetTheme.Editor.ExistingAssetPreviewWidgets",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeExistingUIPreviewTest::RunTest(const FString& Parameters)
{
	int32 Previewed = 0;
	const TArray<UWidgetTheme*> Themes = FWidgetThemeEditorService::Themes();
	TestTrue(TEXT("Existing project themes found"), !Themes.IsEmpty());
	for (UWidgetTheme* Theme : Themes)
	{
		TStrongObjectPtr<UWidgetTheme> PreviewRoot(NewObject<UWidgetTheme>(GetTransientPackage(), NAME_None, RF_Transient));
		FWidgetThemeResolver::SetPreviewOverride(PreviewRoot.Get(), Theme);
		TStrongObjectPtr<UWidgetThemePreviewButton> Button(NewObject<UWidgetThemePreviewButton>(PreviewRoot.Get()));
		Button->SetDesignerFlags(EWidgetDesignFlags::Designing);
		if (!TestTrue(TEXT("Preview button initializes"), Button->Initialize()))
		{
			FWidgetThemeResolver::ClearPreviewOverride(PreviewRoot.Get());
			return false;
		}
		Button->SetTitle(FText::FromString(TEXT("Text Block")));
		Button->TakeWidget();
		TStrongObjectPtr<UCommonTextBlockN> Text(NewObject<UCommonTextBlockN>(PreviewRoot.Get()));
		Text->SetDesignerFlags(EWidgetDesignFlags::Designing);
		Text->SetText(FText::FromString(TEXT("Aa 0123 中文")));
		Text->TakeWidget();
		TStrongObjectPtr<UCommonImageN> Image(NewObject<UCommonImageN>(PreviewRoot.Get()));
		Image->SetDesignerFlags(EWidgetDesignFlags::Designing);
		Image->TakeWidget();
		for (FGameplayTag Tag : FWidgetThemeEditorService::Tags(Theme))
		{
			EWidgetStyleKind Kind;
			if (!FWidgetThemeEditorService::Kind(Tag, Kind))
				continue;
			UClass* StyleClass = Theme->GetStyleClass(Tag, Kind);
			if (!TestNotNull(*Tag.ToString(), StyleClass))
				continue;
			if (Kind == EWidgetStyleKind::Button)
			{
				Button->SetStyleTag(Tag);
				TestEqual(TEXT("Existing button style applied"), Button->GetStyle()->GetClass(), StyleClass);
				Button->SetIsEnabled(false);
				TestFalse(TEXT("Button disabled state applies"), Button->GetIsEnabled());
				Button->SetIsEnabled(true);
				Button->SetPreviewTextVisible(false);
				Button->SetPreviewTextVisible(true);
				TestEqual(TEXT("Preview title preserved across style changes"), Button->GetTitle().ToString(), FString(TEXT("Text Block")));
			}
			else if (Kind == EWidgetStyleKind::Text)
			{
				Text->SetStyleTag(Tag);
				FObjectPropertyBase* StyleProperty = FindFProperty<FObjectPropertyBase>(Text->GetClass(), TEXT("Style"));
				if (TestNotNull(TEXT("Text style property"), StyleProperty))
					TestEqual(TEXT("Existing text style applied"), StyleProperty->GetObjectPropertyValue_InContainer(Text.Get()),
							  static_cast<UObject*>(StyleClass));
			}
			else
			{
				Image->SetStyleTag(Tag);
				TestEqual(TEXT("Existing brush applied"), Image->GetBrush().GetResourceObject(),
						  StyleClass->GetDefaultObject<UWidgetBrushStyle>()->Brush.GetResourceObject());
			}
			++Previewed;
		}
		FWidgetThemeResolver::ClearPreviewOverride(PreviewRoot.Get());
	}
	AddInfo(FString::Printf(TEXT("Constructed button/text/image widgets and applied %d existing theme mappings."), Previewed));
	return Previewed > 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeMoveTest, "WHFramework.WidgetTheme.Editor.RenameAndMovePreserveStyles",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeMoveTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_Move"));
	if (!Fixture.Parent(Theme, Error))
	{
		return false;
	}
	UBlueprint* Style = FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorParentA);
	const FGuid Identity = Theme->Bindings[TAG_ThemeEditorParentA].AssetId;
	const bool Renamed = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
							 .Get()
							 .RenameAssets({FAssetRenameData(TWeakObjectPtr<UObject>(Theme), Fixture.Directory / TEXT("Moved"), FString(TEXT("DA_Renamed")))});
	TestTrue(TEXT("Native rename/move completes"), Renamed);
	TestEqual(TEXT("Private Blueprint retained"), FWidgetThemeEditorService::Blueprint(Theme, TAG_ThemeEditorParentA), Style);
	TestEqual(TEXT("Private identity retained"), Theme->Bindings[TAG_ThemeEditorParentA].AssetId, Identity);
	TestTrue(TEXT("Mapping retained"), FWidgetThemeEditorService::Tags(Theme).Contains(TAG_ThemeEditorParentA));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeSaveReloadTest, "WHFramework.WidgetTheme.Editor.SaveAndReloadDuplication",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeSaveReloadTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Source = Fixture.Theme(TEXT("DA_DiskSource"));
	if (!Fixture.Parent(Source, Error) || !Fixture.Tint(Source, TAG_ThemeEditorParentA, FLinearColor::Red, Error))
	{
		return false;
	}
	UWidgetTheme* Copy = Fixture.Duplicate(Source);
	if (!Copy || Copy->bDuplicateFailed)
	{
		AddError(TEXT("Native duplication did not complete."));
		return false;
	}
	const FSoftObjectPath ThemePath(Copy);
	const FSoftObjectPath StylePath(FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorParentA));
	TArray<UPackage*> Packages{StylePath.ResolveObject()->GetOutermost(), Copy->GetOutermost()};
	for (UPackage* Package : Packages)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!TestTrue(TEXT("Save exact test package"), UPackage::SavePackage(Package, nullptr, *Filename, Args)))
		{
			return false;
		}
	}
	FText ReloadError;
	if (!TestTrue(TEXT("Reload from disk"), UPackageTools::ReloadPackages(Packages, ReloadError, EReloadPackagesInteractionMode::AssumePositive)))
	{
		AddError(ReloadError.ToString());
		return false;
	}
	UWidgetTheme* Reloaded = Cast<UWidgetTheme>(ThemePath.TryLoad());
	Fixture.Themes.Add(Reloaded);
	if (!TestNotNull(TEXT("Reloaded theme"), Reloaded))
	{
		return false;
	}
	TestEqual(TEXT("Reloaded private class path"), FSoftObjectPath(FWidgetThemeEditorService::Blueprint(Reloaded, TAG_ThemeEditorParentA)), StylePath);
	TestEqual(TEXT("Reloaded style values"), Fixture.Brush(Reloaded)->Brush.TintColor.GetSpecifiedColor(), FLinearColor::Red);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeUnusedTagTest, "WHFramework.WidgetTheme.Editor.UnusedTagCleanup",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeUnusedTagTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_UnusedTag"));
	const FString Name = TEXT("TestCleanup_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!FWidgetThemeEditorService::Create(Theme, WidgetStyleTags::Brush, Name, EWidgetStyleKind::Brush, TEXT("/Game/Themes"), false, Error))
	{
		AddError(Error);
		return false;
	}
	const FString FullName = TEXT("Style.Brush.") + Name;
	const FGameplayTag StyleTag = FGameplayTag::RequestGameplayTag(FName(*FullName));
	TestTrue(TEXT("Remove owned style"), FWidgetThemeEditorService::RemoveBinding(Theme, StyleTag, Error));
	TestFalse(TEXT("Unused ordinary Tag deleted"), FGameplayTag::RequestGameplayTag(FName(*FullName), false).IsValid());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeRenameReferenceTest, "WHFramework.WidgetTheme.Editor.RenameReferencesAndRedirect",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeRenameReferenceTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Theme = Fixture.Theme(TEXT("DA_RenameReferences"));
	const FString Name = TEXT("TestRename_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!FWidgetThemeEditorService::Create(Theme, WidgetStyleTags::Brush, Name, EWidgetStyleKind::Brush, TEXT("/Game/Themes"), false, Error))
	{
		AddError(Error);
		return false;
	}
	const FString OldName = TEXT("Style.Brush.") + Name;
	const FString NewName = OldName + TEXT("Renamed");
	const FGameplayTag OldTag = FGameplayTag::RequestGameplayTag(FName(*OldName));
	UBlueprint* Style = FWidgetThemeEditorService::Blueprint(Theme, OldTag);
	UWidgetTheme* Borrower = Fixture.Theme(TEXT("DA_RenameBorrower"));
	if (!FWidgetThemeEditorService::Associate(Borrower, WidgetStyleTags::Brush, TEXT("ThemeEditorAlias"), Theme, OldTag, Error))
	{
		AddError(Error);
		return false;
	}
	Fixture.TextFiles = {FPaths::ProjectDir() / TEXT("Source") / (Name + TEXT(".h")), FPaths::ProjectConfigDir() / (Name + TEXT(".ini"))};
	for (const FString& File : Fixture.TextFiles)
	{
		FFileHelper::SaveStringToFile(TEXT("// ") + OldName + TEXT("\n"), *File);
	}
	if (!TestTrue(TEXT("Rename updates all reference forms"), FWidgetThemeEditorService::RenameStyle(Theme, OldTag, Name + TEXT("Renamed"), Error)))
	{
		AddError(Error);
		return false;
	}
	const FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*NewName));
	TestFalse(TEXT("Old theme key removed"), FWidgetThemeEditorService::Tags(Theme).Contains(OldTag));
	TestEqual(TEXT("Blueprint identity retained"), FWidgetThemeEditorService::Blueprint(Theme, NewTag), Style);
	TestEqual(TEXT("Shared source Tag updated"), Borrower->Bindings[TAG_ThemeEditorAlias].SourceTag, NewTag);
	for (const FString& File : Fixture.TextFiles)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *File);
		TestTrue(TEXT("Source and config reference rewritten"), Text.Contains(NewName));
		TestFalse(TEXT("Old reference replaced exactly"), Text.Contains(OldName + TEXT("\n")));
	}
	TestTrue(TEXT("Old serialized name redirects"),
			 GetDefault<UGameplayTagsSettings>()->GameplayTagRedirects.ContainsByPredicate(
				 [&](const FGameplayTagRedirect& Redirect) { return Redirect.OldTagName == FName(*OldName) && Redirect.NewTagName == FName(*NewName); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemeBlueprintFieldDuplicateTest, "WHFramework.WidgetTheme.Editor.BlueprintFieldsInDuplication",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FThemeBlueprintFieldDuplicateTest::RunTest(const FString& Parameters)
{
	using namespace WidgetThemeEditorTests;
	FFixture Fixture;
	FString Error;
	UWidgetTheme* Source = Fixture.Theme(TEXT("DA_CustomFields"));
	if (!Fixture.Parent(Source, Error))
	{
		return false;
	}
	UBlueprint* Parent = FWidgetThemeEditorService::Blueprint(Source, TAG_ThemeEditorParentA);
	FEdGraphPinType Type;
	Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
	Type.PinSubCategoryObject = TBaseStructure<FSlateBrush>::Get();
	if (!TestTrue(TEXT("Add editable Blueprint brush field"), FBlueprintEditorUtils::AddMemberVariable(Parent, TEXT("ExtraBrush"), Type)))
	{
		return false;
	}
	Parent->NewVariables.Last().PropertyFlags |= CPF_Edit;
	FWidgetThemeEditorService::CompileStyle(Source, TAG_ThemeEditorParentA, Error);
	FStructProperty* Property = CastField<FStructProperty>(Parent->GeneratedClass->FindPropertyByName(TEXT("ExtraBrush")));
	if (!TestNotNull(TEXT("Blueprint brush field compiled"), Property))
	{
		return false;
	}
	Property->ContainerPtrToValuePtr<FSlateBrush>(Parent->GeneratedClass->GetDefaultObject())->ImageSize = FVector2D(67, 29);
	FWidgetThemeEditorService::CompileStyle(Source, TAG_ThemeEditorParentA, Error);
	if (!FWidgetThemeEditorService::Create(Source, TAG_ThemeEditorParentA, TEXT("Child"), EWidgetStyleKind::Brush, TEXT("/Game/Themes"), true, Error))
	{
		AddError(Error);
		return false;
	}
	UWidgetTheme* Copy = Fixture.Duplicate(Source);
	if (!TestNotNull(TEXT("Custom-field theme duplicate"), Copy) || Copy->bDuplicateFailed)
	{
		return false;
	}
	UBlueprint* Child = FWidgetThemeEditorService::Blueprint(Copy, TAG_ThemeEditorChild);
	FStructProperty* Inherited = CastField<FStructProperty>(Child->GeneratedClass->FindPropertyByName(TEXT("ExtraBrush")));
	if (!TestNotNull(TEXT("Inherited Blueprint field retained"), Inherited))
	{
		return false;
	}
	TestEqual(TEXT("Inherited field value preserved"),
			  FVector2D(Inherited->ContainerPtrToValuePtr<FSlateBrush>(Child->GeneratedClass->GetDefaultObject())->ImageSize), FVector2D(67, 29));
	return true;
}
#endif
