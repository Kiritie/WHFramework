#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetTheme.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Widget/Style/WidgetBrushStyle.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "BlueprintEditorLibrary.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "UObject/StrongObjectPtr.h"
#include "ObjectTools.h"
#include "Interfaces/IProjectManager.h"
#include "ProjectDescriptor.h"
#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "Settings/ProjectPackagingSettings.h"
#include "GameplayTagsEditorModule.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "IAssetTools.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectRedirector.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

#include "Widget/Theme/WidgetStyleTags.h"
#include "Editor.h"
#include "WidgetStylePropertyEditing.h"
#include "WidgetStyleClipboard.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/ArchiveReplaceObjectRef.h"

bool FWidgetThemeEditorService::bBusy = false;
namespace WidgetThemeEdit
{
	static TArray<FString> ProjectContentMounts();
	static TArray<FAssetData> ProjectAssets(bool StylesOnly);
	static FString MountRoot(const FString& Path);
	static TSet<UPackage*> ReferenceChangedPackages;
	static TArray<UWidgetTheme*> AllThemes()
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UWidgetTheme::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> Assets;
		auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		for (const FString& Root : ProjectContentMounts())
		{
			Filter.PackagePaths.Add(FName(*Root));
		}
		Filter.bRecursivePaths = true;
		Registry.GetAssets(Filter, Assets);
		TArray<UWidgetTheme*> Out;
		for (const FAssetData& A : Assets)
		{
			if (auto* T = Cast<UWidgetTheme>(A.GetAsset()))
			{
				Out.AddUnique(T);
			}
		}
		for (TObjectIterator<UWidgetTheme> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject | RF_Transient) && It->GetOutermost() != GetTransientPackage() &&
				ProjectContentMounts().Contains(MountRoot(It->GetOutermost()->GetName())))
			{
				Out.AddUnique(*It);
			}
		}
		return Out;
	}
	static bool Writable(UPackage* Package, FString& Report)
	{
		FString File;
		if (FPackageName::DoesPackageExist(Package->GetName(), &File) && IFileManager::Get().IsReadOnly(*File))
		{
			Report += TEXT("Read-only; check out/unlock first: ") + File + TEXT("\n");
			return false;
		}
		return true;
	}
	static FString MountRoot(const FString& Path)
	{
		int32 End = INDEX_NONE;
		if (!Path.FindChar(TEXT('/'), End))
		{
			return FString();
		}
		End = Path.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
		return End == INDEX_NONE ? Path : Path.Left(End);
	}
	static FString ThemeRoot(const UObject* Asset)
	{
		return MountRoot(Asset->GetOutermost()->GetName()) / TEXT("__ExternalAsset__/Styles");
	}
	static bool NewPackage(const FString& DestinationRoot, FString& Package, FGuid& ID, FString& Report)
	{
		if (!FWidgetThemeEditorService::GetCreationRoots().Contains(DestinationRoot))
		{
			Report = TEXT("Select Game or a plugin; managed styles are stored under __ExternalAsset__/Styles.");
			return false;
		}
		const FString R = DestinationRoot;
		const FString Mount = MountRoot(DestinationRoot) + TEXT("/");
		if (!FPackageName::MountPointExists(Mount))
		{
			Report = TEXT("Missing mount: ") + Mount;
			return false;
		}
		auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		do
		{
			ID = FGuid::NewGuid();
			Package = R / ID.ToString(EGuidFormats::Digits);
		} while (FindPackage(nullptr, *Package) || FPackageName::DoesPackageExist(Package) ||
				 Registry.GetAssetByObjectPath(FSoftObjectPath(Package + TEXT(".") + FPackageName::GetShortName(Package))).IsValid());
		FString Directory = FPaths::GetPath(FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension()));
		if (!IFileManager::Get().MakeDirectory(*Directory, true))
		{
			Report = TEXT("Cannot create asset directory: ") + Directory;
			return false;
		}
		return true;
	}
	static bool Compile(UBlueprint* BP, FString& Report)
	{
		FCompilerResultsLog Log;
		FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Log);
		if (Log.NumErrors || !BP->GeneratedClass || BP->Status == BS_Error)
		{
			Report += TEXT("Compile failed: ") + BP->GetPathName() + TEXT("; see compiler log.\n");
			return false;
		}
		return true;
	}
	static bool ForbiddenFrameworkReference(const FString& Path, const FString& OwnerRoot)
	{
		if (OwnerRoot == TEXT("/Game") || Path.IsEmpty() || Path.StartsWith(OwnerRoot + TEXT("/")) || Path.StartsWith(TEXT("/WHFramework/")) ||
			Path.StartsWith(TEXT("/Engine/")))
		{
			return false;
		}
		const auto* Settings = GetDefault<UWidgetThemeSettings>();
		if (Path.StartsWith(TEXT("/Script/")))
		{
			FString Module = Path.Mid(8);
			int32 Dot;
			if (Module.FindChar(TEXT('.'), Dot))
			{
				Module = Module.Left(Dot);
			}
			const auto Project = IProjectManager::Get().GetCurrentProject();
			if (Project)
			{
				for (const FModuleDescriptor& M : Project->Modules)
				{
					if (M.Name == FName(*Module))
					{
						return true;
					}
				}
			}
			const TSharedPtr<IPlugin> ModulePlugin = IPluginManager::Get().GetModuleOwnerPlugin(FName(*Module));
			if (ModulePlugin && MountRoot(ModulePlugin->GetMountedAssetPath()) == OwnerRoot)
			{
				return false;
			}
			return Module != TEXT("InterchangeEngine") && !Settings->AllowedFrameworkNativeModules.Contains(FName(*Module));
		}
		if (Path.StartsWith(TEXT("/Game/")))
		{
			return true;
		}
		for (const FString& Root : Settings->AllowedFrameworkPluginRoots)
		{
			if (Path.StartsWith(Root + TEXT("/")))
			{
				return false;
			}
		}
		TArray<FString> Pending{OwnerRoot.Mid(1)};
		TSet<FString> VisitedPlugins;
		while (!Pending.IsEmpty())
		{
			const FString Name = Pending.Pop(EAllowShrinking::No);
			if (VisitedPlugins.Contains(Name))
			{
				continue;
			}
			VisitedPlugins.Add(Name);
			const TSharedPtr<IPlugin> OwnerPlugin = IPluginManager::Get().FindPlugin(Name);
			if (!OwnerPlugin)
			{
				continue;
			}
			if (Path.StartsWith(OwnerPlugin->GetMountedAssetPath()))
			{
				return false;
			}
			for (const FPluginReferenceDescriptor& Dependency : OwnerPlugin->GetDescriptor().Plugins)
			{
				if (Dependency.bEnabled)
				{
					Pending.Add(Dependency.Name);
				}
			}
		}
		return Path.StartsWith(TEXT("/"));
	}
	static bool IsProjectReference(FProperty* P, const void* Value, const FString& Path, FString& Report, TSet<const UObject*>* Visited = nullptr)
	{
		if (P->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient))
		{
			return false;
		}
		bool Bad = false;
		auto Check = [&](const FString& Asset)
		{
			if (ForbiddenFrameworkReference(Asset, MountRoot(Path)))
			{
				Report += TEXT("Forbidden framework dependency: ") + Path + TEXT(" -> ") + Asset + TEXT("\n");
				Bad = true;
			}
		};
		if (const auto* Soft = CastField<FSoftObjectProperty>(P))
		{
			Check(Soft->GetPropertyValue(Value).ToSoftObjectPath().ToString());
		}
		else if (const auto* Obj = CastField<FObjectPropertyBase>(P))
		{
			if (UObject* O = Obj->GetObjectPropertyValue(Value))
			{
				Check(O->GetPathName());
				for (UClass* C = Cast<UClass>(O) ? CastChecked<UClass>(O) : O->GetClass(); C; C = C->GetSuperClass())
				{
					Check(C->GetPathName());
				}
				if (Visited &&
					(!O->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) && !O->GetOutermost()->GetName().StartsWith(TEXT("/Script/")) &&
					 !O->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"))) &&
					!Visited->Contains(O))
				{
					Visited->Add(O);
					for (TFieldIterator<FProperty> Child(O->GetClass()); Child; ++Child)
					{
						for (int32 I = 0; I < Child->ArrayDim; ++I)
						{
							Bad |= IsProjectReference(*Child, Child->ContainerPtrToValuePtr<void>(O, I),
													  Path + TEXT("->") + O->GetName() + TEXT(".") + Child->GetName(), Report, Visited);
						}
					}
					if (UClass* Class = Cast<UClass>(O))
					{
						UObject* CDO = Class->GetDefaultObject();
						if (!Visited->Contains(CDO))
						{
							Visited->Add(CDO);
							for (TFieldIterator<FProperty> Child(Class); Child; ++Child)
							{
								for (int32 I = 0; I < Child->ArrayDim; ++I)
								{
									Bad |= IsProjectReference(*Child, Child->ContainerPtrToValuePtr<void>(CDO, I), Path + TEXT(".CDO.") + Child->GetName(),
															  Report, Visited);
								}
							}
						}
					}
				}
			}
		}
		else if (const auto* S = CastField<FStructProperty>(P))
		{
			if (S->Struct == TBaseStructure<FSoftObjectPath>::Get())
			{
				Check(static_cast<const FSoftObjectPath*>(Value)->ToString());
			}
			else
			{
				for (TFieldIterator<FProperty> It(S->Struct); It; ++It)
				{
					for (int32 I = 0; I < It->ArrayDim; ++I)
					{
						Bad |= IsProjectReference(*It, It->ContainerPtrToValuePtr<void>(Value, I), Path + TEXT(".") + It->GetName(), Report, Visited);
					}
				}
			}
		}
		else if (const auto* A = CastField<FArrayProperty>(P))
		{
			FScriptArrayHelper H(A, Value);
			for (int32 I = 0; I < H.Num(); ++I)
			{
				Bad |= IsProjectReference(A->Inner, H.GetRawPtr(I), Path + FString::Printf(TEXT("[%d]"), I), Report, Visited);
			}
		}
		else if (const auto* M = CastField<FMapProperty>(P))
		{
			FScriptMapHelper H(M, Value);
			for (int32 I = 0; I < H.GetMaxIndex(); ++I)
			{
				if (H.IsValidIndex(I))
				{
					Bad |= IsProjectReference(M->KeyProp, H.GetKeyPtr(I), Path + TEXT(".Key"), Report, Visited);
					Bad |= IsProjectReference(M->ValueProp, H.GetValuePtr(I), Path + TEXT(".Value"), Report, Visited);
				}
			}
		}
		else if (const auto* SetProperty = CastField<FSetProperty>(P))
		{
			FScriptSetHelper H(SetProperty, Value);
			for (int32 I = 0; I < H.GetMaxIndex(); ++I)
			{
				if (H.IsValidIndex(I))
				{
					Bad |= IsProjectReference(SetProperty->ElementProp, H.GetElementPtr(I), Path + TEXT(".Element"), Report, Visited);
				}
			}
		}
		return Bad;
	}
	static bool DependencySafe(UObject* Asset, FString& Report, bool bAsFramework = false)
	{
		if (!Asset ||
			(!bAsFramework && (Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) || Asset->GetOutermost()->GetName().StartsWith(TEXT("/Script/")) ||
							   Asset->GetOutermost()->GetName().StartsWith(TEXT("/Engine/")))))
		{
			return true;
		}
		bool Bad = false;
		TSet<const UObject*> Visited;
		Visited.Add(Asset);
		for (UClass* C = Cast<UClass>(Asset) ? CastChecked<UClass>(Asset) : Asset->GetClass(); C; C = C->GetSuperClass())
		{
			if (ForbiddenFrameworkReference(C->GetPathName(), MountRoot(Asset->GetOutermost()->GetName())))
			{
				Report += TEXT("Forbidden framework native/class dependency: ") + C->GetPathName() + TEXT("\n");
				Bad = true;
			}
		}
		for (TFieldIterator<FProperty> P(Asset->GetClass()); P; ++P)
		{
			for (int32 I = 0; I < P->ArrayDim; ++I)
			{
				Bad |= IsProjectReference(*P, P->ContainerPtrToValuePtr<void>(Asset, I), Asset->GetPathName() + TEXT(".") + P->GetName(), Report, &Visited);
			}
		}
		auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TSet<FName> Seen;
		TArray<FName> Queue{Asset->GetOutermost()->GetFName()};
		while (!Queue.IsEmpty())
		{
			FName P = Queue.Pop(EAllowShrinking::No);
			if (Seen.Contains(P))
			{
				continue;
			}
			Seen.Add(P);
			if (ForbiddenFrameworkReference(P.ToString(), MountRoot(Asset->GetOutermost()->GetName())))
			{
				Report += TEXT("Forbidden transitive framework dependency: ") + P.ToString() + TEXT("\n");
				Bad = true;
			}
			TArray<FName> Dependencies;
			Registry.GetDependencies(P, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			Queue.Append(Dependencies);
		}
		return !Bad;
	}
	static int32 ManagedOwnerCount(UClass* Class)
	{
		int32 Count = 0;
		for (UWidgetTheme* Theme : AllThemes())
		{
			for (const auto& Binding : Theme->Bindings)
			{
				EWidgetStyleKind Kind;
				if (Binding.Value.bManaged && FWidgetThemeEditorService::Kind(Binding.Key, Kind) && Theme->GetStyleClass(Binding.Key, Kind) == Class)
				{
					++Count;
				}
			}
		}
		return Count;
	}
	static void Abandon(UObject* Asset)
	{
		if (!Asset)
		{
			return;
		}
		FAssetRegistryModule::AssetDeleted(Asset);
		Asset->ClearFlags(RF_Public | RF_Standalone);
		const FString DiscardName = TEXT("AbandonedThemeAsset_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Asset->Rename(*DiscardName, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Asset->MarkAsGarbage();
	}
	static bool RenameWithRedirectors(const TArray<FAssetRenameData>& Renames, FString& Report)
	{
		struct FSource
		{
			TWeakObjectPtr<UObject> Asset;
			UPackage* Package;
			FName AssetName;
			FName GeneratedClassName;
			bool bAddedRoot;
		};
		TArray<FSource> Sources;
		for (const FAssetRenameData& Rename : Renames)
		{
			UObject* Asset = Rename.Asset.Get();
			if (!Asset)
			{
				Report += TEXT("Move source is no longer available.\n");
				return false;
			}
			UPackage* Package = Asset->GetOutermost();
			const UBlueprint* BlueprintAsset = Cast<UBlueprint>(Asset);
			Sources.Add({Asset, Package, Asset->GetFName(),
						 BlueprintAsset && BlueprintAsset->GeneratedClass ? BlueprintAsset->GeneratedClass->GetFName() : NAME_None, !Package->IsRooted()});
		}
		struct FPackageRoots
		{
			TArray<FSource>& Sources;
			~FPackageRoots()
			{
				for (const FSource& Source : Sources)
				{
					if (Source.bAddedRoot)
					{
						Source.Package->RemoveFromRoot();
					}
				}
			}
		} Roots{Sources};
		for (const FSource& Source : Sources)
		{
			if (Source.bAddedRoot)
			{
				Source.Package->AddToRoot();
			}
		}
		auto* Settings = GetMutableDefault<UWidgetThemeSettings>();
		const TSoftObjectPtr<UWidgetTheme> PreviousDefault = Settings->DefaultTheme;
		const TArray<TSoftObjectPtr<UWidgetTheme>> PreviousAvailable = Settings->AvailableThemes;
		TMap<FSoftObjectPath, FSoftObjectPath> ConfigPaths;
		for (const FAssetRenameData& Rename : Renames)
		{
			ConfigPaths.Add(FSoftObjectPath(Rename.Asset.Get()), FSoftObjectPath(Rename.NewPackagePath / Rename.NewName + TEXT(".") + Rename.NewName));
		}
		auto RewriteSetting = [&ConfigPaths](auto& Value)
		{
			if (const FSoftObjectPath* NewPath = ConfigPaths.Find(Value.ToSoftObjectPath()))
			{
				Value = *NewPath;
			}
		};
		RewriteSetting(Settings->DefaultTheme);
		for (auto& Path : Settings->AvailableThemes)
		{
			RewriteSetting(Path);
		}
		bool bSucceeded = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RenameAssets(Renames);
		if (!bSucceeded)
		{
			Settings->DefaultTheme = PreviousDefault;
			Settings->AvailableThemes = PreviousAvailable;
		}
		else if (!Settings->TryUpdateDefaultConfigFile())
		{
			Report += TEXT("Assets moved, but rewritten theme settings could not be saved.\n");
			bSucceeded = false;
		}
		TArray<FString> ChangedFiles;
		for (const FSource& Source : Sources)
		{
			UObject* Asset = Source.Asset.Get();
			if (!Asset || Asset->GetOutermost() == Source.Package)
			{
				bSucceeded = false;
				continue;
			}
			auto PreservePath = [&](FName Name, UObject* Destination)
			{
				if (Name.IsNone() || !Destination)
				{
					return;
				}
				UObject* Existing = FindObject<UObject>(Source.Package, *Name.ToString());
				UObjectRedirector* Redirector = Cast<UObjectRedirector>(Existing);
				if (Existing && (!Redirector || Redirector->DestinationObject != Destination))
				{
					Report += TEXT("Old-path redirect collision: ") + Existing->GetPathName() + TEXT("\n");
					bSucceeded = false;
					return;
				}
				if (!Redirector)
				{
					Redirector = NewObject<UObjectRedirector>(Source.Package, Name, RF_Public | RF_Standalone | RF_Transactional);
					Redirector->DestinationObject = Destination;
					FAssetRegistryModule::AssetCreated(Redirector);
				}
				Source.Package->MarkPackageDirty();
			};
			PreservePath(Source.AssetName, Asset);
			if (const UBlueprint* BlueprintAsset = Cast<UBlueprint>(Asset))
			{
				PreservePath(Source.GeneratedClassName, BlueprintAsset->GeneratedClass);
			}
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			const FString Filename = FPackageName::LongPackageNameToFilename(Source.Package->GetName(), FPackageName::GetAssetPackageExtension());
			const bool bSaved = UPackage::SavePackage(Source.Package, nullptr, *Filename, Args);
			bSucceeded &= bSaved;
			ChangedFiles.Add(Filename);
			ChangedFiles.Add(FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()));
			Report += (bSaved ? TEXT("Redirects saved: ") : TEXT("Redirect save FAILED: ")) + Source.Package->GetName() + TEXT("\n");
		}
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().ScanModifiedAssetFiles(ChangedFiles);
		return bSucceeded;
	}
	static bool RewriteTagText(FString& Text, FGameplayTag Old, FGameplayTag New)
	{
		bool bChanged = false;
		const FString Before = Old.ToString(), After = New.ToString();
		int32 Offset = 0;
		while ((Offset = Text.Find(Before, ESearchCase::CaseSensitive, ESearchDir::FromStart, Offset)) != INDEX_NONE)
		{
			auto Token = [](TCHAR Character) { return FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('.'); };
			const int32 End = Offset + Before.Len();
			if ((Offset == 0 || !Token(Text[Offset - 1])) && (End == Text.Len() || !Token(Text[End])))
			{
				Text = Text.Left(Offset) + After + Text.Mid(End);
				Offset += After.Len();
				bChanged = true;
			}
			else
			{
				Offset = End;
			}
		}
		return bChanged;
	}
	static bool RewriteTag(FProperty* P, void* Value, FGameplayTag Old, FGameplayTag New)
	{
		bool Changed = false;
		if (auto* S = CastField<FStructProperty>(P))
		{
			if (S->Struct == FGameplayTag::StaticStruct())
			{
				auto* Tag = static_cast<FGameplayTag*>(Value);
				if (*Tag == Old)
				{
					*Tag = New;
					return true;
				}
			}
			else
			{
				for (TFieldIterator<FProperty> It(S->Struct); It; ++It)
				{
					for (int32 I = 0; I < It->ArrayDim; ++I)
					{
						Changed |= RewriteTag(*It, It->ContainerPtrToValuePtr<void>(Value, I), Old, New);
					}
				}
			}
		}
		else if (auto* A = CastField<FArrayProperty>(P))
		{
			FScriptArrayHelper H(A, Value);
			for (int32 I = 0; I < H.Num(); ++I)
			{
				Changed |= RewriteTag(A->Inner, H.GetRawPtr(I), Old, New);
			}
		}
		else if (auto* M = CastField<FMapProperty>(P))
		{
			FScriptMapHelper H(M, Value);
			for (int32 I = 0; I < H.GetMaxIndex(); ++I)
			{
				if (H.IsValidIndex(I))
				{
					Changed |= RewriteTag(M->KeyProp, H.GetKeyPtr(I), Old, New);
					Changed |= RewriteTag(M->ValueProp, H.GetValuePtr(I), Old, New);
				}
			}
			if (Changed)
			{
				H.Rehash();
			}
		}
		else if (auto* SetProperty = CastField<FSetProperty>(P))
		{
			FScriptSetHelper H(SetProperty, Value);
			for (int32 I = 0; I < H.GetMaxIndex(); ++I)
			{
				if (H.IsValidIndex(I))
				{
					Changed |= RewriteTag(SetProperty->ElementProp, H.GetElementPtr(I), Old, New);
				}
			}
			if (Changed)
			{
				H.Rehash();
			}
		}
		else if (auto* String = CastField<FStrProperty>(P))
		{
			FString Text = String->GetPropertyValue(Value);
			Changed = RewriteTagText(Text, Old, New);
			if (Changed)
			{
				String->SetPropertyValue(Value, Text);
			}
		}
		else if (auto* Name = CastField<FNameProperty>(P); Name && Name->GetPropertyValue(Value) == Old.GetTagName())
		{
			Name->SetPropertyValue(Value, New.GetTagName());
			Changed = true;
		}
		return Changed;
	}
	static bool DuplicateKeys(FProperty* P, void* Value)
	{
		if (auto* M = CastField<FMapProperty>(P))
		{
			FScriptMapHelper H(M, Value);
			for (int32 I = 0; I < H.GetMaxIndex(); ++I)
			{
				if (H.IsValidIndex(I))
				{
					for (int32 J = I + 1; J < H.GetMaxIndex(); ++J)
					{
						if (H.IsValidIndex(J) && M->KeyProp->Identical(H.GetKeyPtr(I), H.GetKeyPtr(J)))
						{
							return true;
						}
					}
					if (DuplicateKeys(M->KeyProp, H.GetKeyPtr(I)) || DuplicateKeys(M->ValueProp, H.GetValuePtr(I)))
					{
						return true;
					}
				}
			}
		}
		else if (auto* SetProperty = CastField<FSetProperty>(P))
		{
			FScriptSetHelper H(SetProperty, Value);
			for (int32 I = 0; I < H.GetMaxIndex(); ++I)
			{
				if (H.IsValidIndex(I))
				{
					for (int32 J = I + 1; J < H.GetMaxIndex(); ++J)
					{
						if (H.IsValidIndex(J) && SetProperty->ElementProp->Identical(H.GetElementPtr(I), H.GetElementPtr(J)))
						{
							return true;
						}
					}
					if (DuplicateKeys(SetProperty->ElementProp, H.GetElementPtr(I)))
					{
						return true;
					}
				}
			}
		}
		else if (auto* A = CastField<FArrayProperty>(P))
		{
			FScriptArrayHelper H(A, Value);
			for (int32 I = 0; I < H.Num(); ++I)
			{
				if (DuplicateKeys(A->Inner, H.GetRawPtr(I)))
				{
					return true;
				}
			}
		}
		else if (auto* S = CastField<FStructProperty>(P))
		{
			for (TFieldIterator<FProperty> C(S->Struct); C; ++C)
			{
				for (int32 I = 0; I < C->ArrayDim; ++I)
				{
					if (DuplicateKeys(*C, C->ContainerPtrToValuePtr<void>(Value, I)))
					{
						return true;
					}
				}
			}
		}
		return false;
	}
	static bool RenameKeysSafe(UObject* Object, FGameplayTag Old, FGameplayTag New, FString& Report)
	{
		for (TFieldIterator<FProperty> P(Object->GetClass()); P; ++P)
		{
			if (P->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}
			void* Temp = FMemory::Malloc(P->GetSize(), P->GetMinAlignment());
			P->InitializeValue(Temp);
			P->CopyCompleteValue(Temp, P->ContainerPtrToValuePtr<void>(Object));
			bool Collision = false;
			for (int32 I = 0; I < P->ArrayDim; ++I)
			{
				void* Element = static_cast<uint8*>(Temp) + I * P->GetElementSize();
				RewriteTag(*P, Element, Old, New);
				Collision |= DuplicateKeys(*P, Element);
			}
			P->DestroyValue(Temp);
			FMemory::Free(Temp);
			if (Collision)
			{
				Report += TEXT("Rename would merge map/set keys: ") + Object->GetPathName() + TEXT(".") + P->GetName() + TEXT("\n");
				return false;
			}
		}
		return true;
	}
	static TArray<FString> ProjectContentMounts()
	{
		TArray<FString> Out{TEXT("/Game")};
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
		{
			if (Plugin->CanContainContent() && Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project)
			{
				Out.AddUnique(MountRoot(Plugin->GetMountedAssetPath()));
			}
		}
		return Out;
	}
	static TArray<FAssetData> ProjectAssets(bool StylesOnly)
	{
		auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		FARFilter Filter;
		for (const FString& Root : ProjectContentMounts())
		{
			Filter.PackagePaths.Add(FName(*Root));
		}
		Filter.bRecursivePaths = true;
		if (StylesOnly)
		{
			Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
			Filter.bRecursiveClasses = true;
		}
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		if (StylesOnly)
		{
			TArray<FTopLevelAssetPath> Bases;
			for (EWidgetStyleKind Type : WidgetStyleTags::Kinds())
			{
				Bases.Add(FWidgetThemeResolver::NativeBase(Type)->GetClassPathName());
			}
			TSet<FTopLevelAssetPath> Derived;
			Registry.GetDerivedClassNames(Bases, {}, Derived);
			Assets.RemoveAll(
				[&](const FAssetData& Asset)
				{
					FString Generated;
					return !Asset.GetTagValue(TEXT("GeneratedClass"), Generated) ||
						   !Derived.Contains(FTopLevelAssetPath(FPackageName::ExportTextPathToObjectPath(Generated)));
				});
		}
		else
		{
			TSet<FName> StylePackages;
			for (const FAssetData& Style : ProjectAssets(true))
			{
				StylePackages.Add(Style.PackageName);
			}
			Assets.RemoveAll(
				[&](const FAssetData& Asset)
				{
					if (StylePackages.Contains(Asset.PackageName))
					{
						return false;
					}
					const FString Extension = Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName() ? FPackageName::GetMapPackageExtension()
																												: FPackageName::GetAssetPackageExtension();
					TArray<uint8> Bytes;
					if (!FFileHelper::LoadFileToArray(Bytes, *FPackageName::LongPackageNameToFilename(Asset.PackageName.ToString(), Extension)))
					{
						return false;
					}
					const uint8 Narrow[]{'S', 't', 'y', 'l', 'e', '.'};
					const uint8 Wide[]{'S', 0, 't', 0, 'y', 0, 'l', 0, 'e', 0, '.', 0};
					for (int32 Index = 0; Index + UE_ARRAY_COUNT(Narrow) <= Bytes.Num(); ++Index)
					{
						if (Bytes[Index] != 'S')
						{
							continue;
						}
						if (FMemory::Memcmp(Bytes.GetData() + Index, Narrow, sizeof(Narrow)) == 0 ||
							(Index + UE_ARRAY_COUNT(Wide) <= Bytes.Num() && FMemory::Memcmp(Bytes.GetData() + Index, Wide, sizeof(Wide)) == 0))
						{
							return false;
						}
					}
					return true;
				});
		}
		Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetObjectPathString() < B.GetObjectPathString(); });
		return Assets;
	}
}
using namespace WidgetThemeEdit;

bool FWidgetThemeEditorService::IsBusy()
{
	return bBusy;
}

UBlueprint* FWidgetThemeEditorService::CreateForTag(UWidgetTheme* Theme, const FString& FullTag, UClass* ParentClass, const FString& DestinationRoot, FString& Report)
{
	if (!Theme || !ParentClass || !GetCreationRoots().Contains(DestinationRoot) || !RegisterTag(FullTag, DestinationRoot, Report))
	{
		return nullptr;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*FullTag));
	EWidgetStyleKind Type;
	if (!Kind(Tag, Type) || WidgetStyleTags::IsRoot(Tag) || Tags(Theme).Contains(Tag) ||
		!ParentClass->IsChildOf(FWidgetThemeResolver::NativeBase(Type)) ||
		ForbiddenFrameworkReference(ParentClass->GetPathName(), MountRoot(DestinationRoot)))
	{
		Report = TEXT("Style type, parent, ownership or destination is invalid.");
		return nullptr;
	}
	FString Package;
	FGuid Identity;
	if (!Writable(Theme->GetOutermost(), Report) || !NewPackage(DestinationRoot, Package, Identity, Report))
	{
		return nullptr;
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "CreateForTag", "Create managed style"));
	Theme->Modify();
	UBlueprint* Style = FKismetEditorUtilities::CreateBlueprint(ParentClass, CreatePackage(*Package), FName(*FPackageName::GetShortName(Package)),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("WidgetThemeEditor"));
	if (!Style || !Compile(Style, Report) || !DependencySafe(Style->GeneratedClass->GetDefaultObject(), Report) ||
		!Theme->SetStyleClass(Tag, Type, Style->GeneratedClass))
	{
		Abandon(Style);
		Transaction.Cancel();
		return nullptr;
	}
	FWidgetStyleBinding Binding;
	Binding.bManaged = true;
	Binding.AssetId = Identity;
	Theme->Bindings.Add(Tag, Binding);
	FAssetRegistryModule::AssetCreated(Style);
	Style->MarkPackageDirty();
	Theme->MarkPackageDirty();
	FWidgetThemeResolver::NotifyThemeChanged();
	return Style;
}
UClass* FWidgetThemeEditorService::CreationBase(EWidgetStyleKind Kind)
{
	return FWidgetThemeResolver::DefaultStyle(Kind);
}
TArray<FGameplayTag> FWidgetThemeEditorService::Tags(const UWidgetTheme* T)
{
	TArray<FGameplayTag> Out;
	if (!T)
	{
		return Out;
	}
	for (EWidgetStyleKind Type : WidgetStyleTags::Kinds())
	{
		Out.Append(T->GetStyleTags(Type));
	}
	Out.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });
	return Out;
}
UBlueprint* FWidgetThemeEditorService::Blueprint(UWidgetTheme* T, FGameplayTag Tag)
{
	EWidgetStyleKind K;
	UClass* C = T && Kind(Tag, K) ? T->ResolveStyleClass(Tag, K) : nullptr;
	return C ? Cast<UBlueprint>(C->ClassGeneratedBy) : nullptr;
}
TArray<FProperty*> FWidgetThemeEditorService::EditableProperties(UClass* C)
{
	TArray<FProperty*> Out;
	if (!C)
	{
		return Out;
	}
	for (TFieldIterator<FProperty> P(C); P; ++P)
	{
		if (P->HasAnyPropertyFlags(CPF_Edit) && !P->HasAnyPropertyFlags(CPF_Transient | CPF_DisableEditOnTemplate))
		{
			Out.Add(*P);
		}
	}
	return Out;
}
TArray<UWidgetTheme*> FWidgetThemeEditorService::Themes()
{
	return AllThemes();
}
FString FWidgetThemeEditorService::DescribeTag(const UWidgetTheme* Theme, FGameplayTag Tag)
{
	EWidgetStyleKind Kind;
	UClass* Class = Theme && FWidgetThemeEditorService::Kind(Tag, Kind) ? Theme->ResolveStyleClass(Tag, Kind) : nullptr;
	UClass* ParentClass = Class ? Class->GetSuperClass() : nullptr;
	FString Parent = GetPathNameSafe(ParentClass);
	bool bFoundManagedParent = false;
	FString Owner = Theme ? Theme->GetPathName() : FString();
	TArray<const UWidgetTheme*> Candidates;
	TSet<const UWidgetTheme*> Visited;
	if (Theme)
	{
		Candidates.Add(Theme);
	}
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const UWidgetTheme* Candidate = Candidates[Index];
		if (!Candidate || Visited.Contains(Candidate))
		{
			continue;
		}
		Visited.Add(Candidate);
		if (Candidate->ParentTheme)
		{
			Candidates.Add(Candidate->ParentTheme);
		}
		for (const auto& SourceBinding : Candidate->Bindings)
		{
			if (SourceBinding.Value.SourceTheme)
			{
				Candidates.Add(SourceBinding.Value.SourceTheme);
			}
		}
		for (FGameplayTag CandidateTag : Tags(Candidate))
		{
			EWidgetStyleKind CandidateKind;
			if (!FWidgetThemeEditorService::Kind(CandidateTag, CandidateKind))
			{
				continue;
			}
			const FWidgetStyleBinding* Binding = Candidate->Bindings.Find(CandidateTag);
			if (Candidate->GetStyleClass(CandidateTag, CandidateKind) == Class && Binding && Binding->bManaged)
			{
				Owner = Candidate->GetPathName();
			}
			if (Candidate->GetStyleClass(CandidateTag, CandidateKind) == ParentClass && (!bFoundManagedParent || (Binding && Binding->bManaged)))
			{
				Parent = Candidate->GetPathName() + TEXT(" | ") + CandidateTag.ToString();
				bFoundManagedParent |= Binding && Binding->bManaged;
			}
		}
	}
	return TEXT("Theme: ") + GetPathNameSafe(Theme) + TEXT("\nOriginal owner: ") + Owner + TEXT("\nStyle Blueprint: ") +
		   (Class && Class->ClassGeneratedBy ? Class->ClassGeneratedBy->GetPathName() : GetPathNameSafe(Class)) + TEXT("\nTag parent: ") +
		   Tag.RequestDirectParent().ToString() + TEXT("\nStyle parent: ") + Parent;
}
bool FWidgetThemeEditorService::GetTagSourceFilename(const FString& OwnerPath, FString& Filename, FString& Report)
{
	Filename.Reset();
	const FString Owner = MountRoot(OwnerPath);
	if (Owner == TEXT("/Game"))
	{
		Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("Tags") / (FString(FApp::GetProjectName()) + TEXT(".WidgetStyles.ini")));
		return true;
	}
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->CanContainContent() && Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project && MountRoot(Plugin->GetMountedAssetPath()) == Owner)
		{
			Filename = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Config/Tags") / (Plugin->GetName() + TEXT(".WidgetStyles.ini")));
			return true;
		}
	}
	Report = TEXT("Select Game or an enabled project content plugin for the Tag definition.");
	return false;
}

bool FWidgetThemeEditorService::RegisterTag(const FString& Name, const FString& OwnerPath, FString& Report)
{
	bool bValidRoot = false;
	for (EWidgetStyleKind Type : WidgetStyleTags::Kinds())
	{
		bValidRoot |= Name.StartsWith(WidgetStyleTags::Root(Type).ToString() + TEXT("."));
	}
	if (!bValidRoot)
	{
		Report = TEXT("A style Tag must be below a supported Style type root.");
		return false;
	}
	if (FGameplayTag::RequestGameplayTag(FName(*Name), false).IsValid())
	{
		return true;
	}
	FString Filename;
	if (!GetTagSourceFilename(OwnerPath, Filename, Report))
	{
		return false;
	}
	const FString Directory = FPaths::GetPath(Filename);
	const FName SourceName(*FPaths::GetCleanFilename(Filename));
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		Report = TEXT("Could not create the owning module Tag directory: ") + Directory;
		return false;
	}
	if (!UGameplayTagsManager::Get().FindTagSource(SourceName) && !IGameplayTagsEditorModule::Get().AddNewGameplayTagSource(SourceName.ToString(), Directory))
	{
		Report = TEXT("Could not create the owning module Tag source: ") + Filename;
		return false;
	}
	const FGameplayTagSource* Source = UGameplayTagsManager::Get().FindTagSource(SourceName);
	if (!Source || !Source->SourceTagList || FPaths::ConvertRelativePathToFull(Source->SourceTagList->ConfigFileName) != Filename)
	{
		Report = TEXT("The Tag source name belongs to a different module directory: ") + SourceName.ToString();
		return false;
	}
	if (!IGameplayTagsEditorModule::Get().AddNewGameplayTagToINI(Name, TEXT("Widget theme style"), SourceName))
	{
		Report = TEXT("Could not register the gameplay Tag. Check its name and config write access.");
		return false;
	}
	return FGameplayTag::RequestGameplayTag(FName(*Name), false).IsValid();
}
TArray<FString> FWidgetThemeEditorService::GetCreationRoots()
{
	TArray<FString> Plugins;
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->CanContainContent() && Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project)
		{
			Plugins.Add(Plugin->GetMountedAssetPath() / TEXT("__ExternalAsset__/Styles"));
		}
	}
	Plugins.Sort();
	TArray<FString> Roots{TEXT("/Game/__ExternalAsset__/Styles")};
	Roots.Append(Plugins);
	return Roots;
}
bool FWidgetThemeEditorService::Kind(FGameplayTag Tag, EWidgetStyleKind& Out)
{
	return WidgetStyleTags::Kind(Tag, Out);
}
bool FWidgetThemeEditorService::ValidateName(const FString& Name, FString& Report)
{
	FText Error;
	for (TCHAR Character : Name)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			Report = TEXT("Use letters, digits or underscores in a single name.");
			return false;
		}
	}
	if (Name.IsEmpty() || Name != Name.TrimStartAndEnd() || Name.Contains(TEXT(".")) || !UGameplayTagsManager::Get().IsValidGameplayTagString(Name, &Error))
	{
		Report = TEXT("Enter a single valid name without dots or whitespace. ") + Error.ToString();
		return false;
	}
	return true;
}

bool FWidgetThemeEditorService::Validate(UWidgetTheme* Theme, FString& Report)
{
	if (!Theme || Theme->bDuplicateFailed)
	{
		Report += TEXT("Missing theme or incomplete asset duplication.\n");
		return false;
	}
	FString RuntimeError;
	bool Good = FWidgetThemeResolver::Validate(Theme, RuntimeError);
	if (!Good)
	{
		Report += RuntimeError + TEXT("\n");
	}
	Good &= DependencySafe(Theme, Report);
	TSet<FGameplayTag> Seen;
	for (FGameplayTag Tag : Tags(Theme))
	{
		EWidgetStyleKind Type;
		if (!Kind(Tag, Type) || WidgetStyleTags::IsRoot(Tag) || Seen.Contains(Tag))
		{
			Report += TEXT("Invalid style Tag: ") + Tag.ToString() + TEXT("\n");
			Good = false;
			continue;
		}
		Seen.Add(Tag);
		UClass* StyleClass = Theme->GetStyleClass(Tag, Type);
		const FWidgetStyleBinding* Binding = Theme->Bindings.Find(Tag);
		if (!StyleClass || !StyleClass->IsChildOf(FWidgetThemeResolver::NativeBase(Type)) || !Binding)
		{
			Report += TEXT("Missing style class or ownership: ") + Tag.ToString() + TEXT("\n");
			Good = false;
			continue;
		}
		UBlueprint* Style = Cast<UBlueprint>(StyleClass->ClassGeneratedBy);
		if (!Style || Style->Status == BS_Error || ManagedOwnerCount(StyleClass) != 1)
		{
			Report += TEXT("Style must be a valid Blueprint with one owning theme: ") + Tag.ToString() + TEXT("\n");
			Good = false;
		}
		Good &= DependencySafe(StyleClass->GetDefaultObject(), Report);
	}
	return Good;
}

bool FWidgetThemeEditorService::Create(UWidgetTheme* Theme, FGameplayTag Parent, const FString& Name, EWidgetStyleKind Type, const FString& DestinationRoot,
									   bool bInheritParent, FString& Report)
{
	EWidgetStyleKind ParentType;
	if (!Theme || !ValidateName(Name, Report) || !Kind(Parent, ParentType) || Type != ParentType ||
		(!WidgetStyleTags::IsRoot(Parent) && !Theme->GetStyleTags(Type, true).Contains(Parent)))
	{
		Report += TEXT("Select a matching type root or a style in this theme.\n");
		return false;
	}
	const FString FullName = Parent.ToString() + TEXT(".") + Name;
	const FGameplayTag Existing = FGameplayTag::RequestGameplayTag(FName(*FullName), false);
	if (Existing.IsValid() && Tags(Theme).Contains(Existing))
	{
		Report = TEXT("This theme already contains the name.");
		return false;
	}
	UClass* ParentClass = bInheritParent ? Theme->ResolveStyleClass(Parent, Type) : CreationBase(Type);
	if (!ParentClass || ForbiddenFrameworkReference(ParentClass->GetPathName(), MountRoot(DestinationRoot)))
	{
		Report = TEXT("The selected parent has no style or cannot be referenced from this destination.");
		return false;
	}
	FString Package;
	FGuid Identity;
	if (!Writable(Theme->GetOutermost(), Report) || !NewPackage(DestinationRoot, Package, Identity, Report))
	{
		return false;
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "CreateStyle", "Create style"));
	Theme->Modify();
	UBlueprint* Style =
		FKismetEditorUtilities::CreateBlueprint(ParentClass, CreatePackage(*Package), FName(*FPackageName::GetShortName(Package)), BPTYPE_Normal,
												UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("WidgetThemeEditor"));
	if (Style)
	{
		Style->GetOutermost()->MarkAsFullyLoaded();
	}
	if (!Style || !Compile(Style, Report) || !DependencySafe(Style->GeneratedClass->GetDefaultObject(), Report) || !RegisterTag(FullName, DestinationRoot, Report))
	{
		Abandon(Style);
		Transaction.Cancel();
		return false;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*FullName));
	Theme->SetStyleClass(Tag, Type, Style->GeneratedClass);
	FWidgetStyleBinding Binding;
	Binding.bManaged = true;
	Binding.AssetId = Identity;
	Theme->Bindings.Add(Tag, Binding);
	FAssetRegistryModule::AssetCreated(Style);
	Style->MarkPackageDirty();
	Theme->MarkPackageDirty();
	FWidgetThemeResolver::NotifyThemeChanged();
	Report += TEXT("Style created with native Blueprint inheritance. Save to persist.\n");
	return true;
}

bool FWidgetThemeEditorService::Associate(UWidgetTheme* Theme, FGameplayTag Parent, const FString& Name, UWidgetTheme* SourceTheme, FGameplayTag SourceTag,
										  FString& Report)
{
	EWidgetStyleKind ParentType = EWidgetStyleKind::None;
	EWidgetStyleKind SourceType = EWidgetStyleKind::None;
	if (!Theme || !SourceTheme || !ValidateName(Name, Report) || !Kind(Parent, ParentType) || !Kind(SourceTag, SourceType) || ParentType != SourceType ||
		(!WidgetStyleTags::IsRoot(Parent) && !Tags(Theme).Contains(Parent)))
	{
		Report += TEXT("Select a source theme and a style matching the parent type.\n");
		return false;
	}
	UClass* SourceClass = SourceTheme->GetStyleClass(SourceTag, SourceType);
	const FString FullName = Parent.ToString() + TEXT(".") + Name;
	FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*FullName), false);
	if (!SourceClass || ManagedOwnerCount(SourceClass) != 1 || (Tag.IsValid() && Tags(Theme).Contains(Tag)) ||
		ForbiddenFrameworkReference(SourceClass->GetPathName(), MountRoot(Theme->GetOutermost()->GetName())) || !Writable(Theme->GetOutermost(), Report))
	{
		Report += TEXT("Source ownership, destination name or dependency is invalid.\n");
		return false;
	}
	if (!RegisterTag(FullName, Theme->GetOutermost()->GetName(), Report))
	{
		return false;
	}
	Tag = FGameplayTag::RequestGameplayTag(FName(*FullName));
	FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "AssociateStyle", "Associate style"));
	Theme->Modify();
	Theme->SetStyleClass(Tag, SourceType, SourceClass);
	FWidgetStyleBinding Binding;
	Binding.SourceTheme = SourceTheme;
	Binding.SourceTag = SourceTag;
	Theme->Bindings.Add(Tag, Binding);
	Theme->MarkPackageDirty();
	FWidgetThemeResolver::NotifyThemeChanged();
	Report = TEXT("Shared style associated. Save to persist.");
	return true;
}

bool FWidgetThemeEditorService::CompileStyle(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report)
{
	const FWidgetStyleBinding* Binding = Theme ? Theme->Bindings.Find(Tag) : nullptr;
	UBlueprint* Style = Theme ? Blueprint(Theme, Tag) : nullptr;
	if (!Binding || !Binding->bManaged || !Style)
	{
		Report = TEXT("Select an editable owned style.");
		return false;
	}
	TGuardValue<bool> Busy(bBusy, true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Style);
	if (!Compile(Style, Report))
	{
		return false;
	}
	EWidgetStyleKind Type;
	Kind(Tag, Type);
	Theme->Modify();
	Theme->SetStyleClass(Tag, Type, Style->GeneratedClass);
	Style->MarkPackageDirty();
	Theme->MarkPackageDirty();
	FWidgetThemeResolver::NotifyThemeChanged();
	return true;
}

bool FWidgetThemeEditorService::RenameStyle(UWidgetTheme* Theme, FGameplayTag Tag, const FString& Name, FString& Report)
{
	const FWidgetStyleBinding* Binding = Theme ? Theme->Bindings.Find(Tag) : nullptr;
	if (!Binding || !Binding->bManaged || WidgetStyleTags::IsRoot(Tag) || !ValidateName(Name, Report))
	{
		Report += TEXT("Select an owned style to rename.\n");
		return false;
	}
	const FString FullName = Tag.RequestDirectParent().ToString() + TEXT(".") + Name;
	if (FullName == Tag.ToString() || FGameplayTag::RequestGameplayTag(FName(*FullName), false).IsValid())
	{
		Report = TEXT("Choose a different unused name.");
		return false;
	}
	UBlueprint* Style = Blueprint(Theme, Tag);
	if (!Style || !RegisterTag(FullName, Style->GetOutermost()->GetName(), Report) || !RenameTag(Tag, FGameplayTag::RequestGameplayTag(FName(*FullName)), Report))
	{
		return false;
	}
	return PersistReferenceChanges(Report);
}

namespace WidgetThemeEdit
{
	static FString PropertyType(FProperty* Property)
	{
		return Property->GetCPPType() + FString::Printf(TEXT("|%d"), Property->ArrayDim);
	}
	static bool ReadClipboard(UWidgetTheme* Theme, FGameplayTag Tag, TSharedPtr<FJsonObject>& Payload, FString& Report)
	{
		const FWidgetStyleBinding* Binding = Theme ? Theme->Bindings.Find(Tag) : nullptr;
		UBlueprint* Style = Theme ? FWidgetThemeEditorService::Blueprint(Theme, Tag) : nullptr;
		EWidgetStyleKind Type;
		if (!Binding || !Binding->bManaged || !Style || !FWidgetThemeEditorService::Kind(Tag, Type))
		{
			return false;
		}
		FString Text;
		WidgetStyleClipboard::Read(Text);
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Payload) || !Payload.IsValid())
		{
			return false;
		}
		FString Format;
		double Version;
		double SourceType;
		const TSharedPtr<FJsonObject>* Fields = nullptr;
		if (!Payload->TryGetStringField(TEXT("format"), Format) || Format != TEXT("WHFramework.WidgetStyleProperties") ||
			!Payload->TryGetNumberField(TEXT("version"), Version) || Version != 2 || !Payload->TryGetNumberField(TEXT("kind"), SourceType) ||
			SourceType != static_cast<int32>(Type) || !Payload->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.IsEmpty())
		{
			return false;
		}
		const TArray<FProperty*> Editable = FWidgetThemeEditorService::EditableProperties(Style->GeneratedClass);
		for (const auto& Entry : (*Fields)->Values)
		{
			const TSharedPtr<FJsonObject>* Field = nullptr;
			FString Signature;
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			FProperty* Property = Style->GeneratedClass->FindPropertyByName(FName(*Entry.Key));
			if (!Property || !Editable.Contains(Property) || !Entry.Value->TryGetObject(Field) || !(*Field)->TryGetStringField(TEXT("type"), Signature) ||
				Signature != PropertyType(Property) || !(*Field)->TryGetArrayField(TEXT("values"), Values) || Values->Num() != Property->ArrayDim)
			{
				Report = TEXT("Clipboard fields are incompatible with the selected style.");
				return false;
			}
			void* Trial = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
			Property->InitializeValue(Trial);
			bool Good = true;
			for (int32 Index = 0; Index < Values->Num(); ++Index)
			{
				FString Value;
				Good &= (*Values)[Index]->TryGetString(Value) &&
						Property->ImportText_Direct(*Value, static_cast<uint8*>(Trial) + Index * Property->GetElementSize(),
													Style->GeneratedClass->GetDefaultObject(), PPF_None) != nullptr;
			}
			Property->DestroyValue(Trial);
			FMemory::Free(Trial);
			if (!Good)
			{
				Report = TEXT("Clipboard property data cannot be imported.");
				return false;
			}
		}
		return true;
	}
}

bool FWidgetThemeEditorService::CopyProperties(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report)
{
	UBlueprint* Style = Theme ? Blueprint(Theme, Tag) : nullptr;
	EWidgetStyleKind Type;
	if (!Style || !Style->GeneratedClass || !Kind(Tag, Type))
	{
		Report = TEXT("Select a style to copy.");
		return false;
	}
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("format"), TEXT("WHFramework.WidgetStyleProperties"));
	Payload->SetNumberField(TEXT("version"), 2);
	Payload->SetNumberField(TEXT("kind"), static_cast<int32>(Type));
	TSharedRef<FJsonObject> Fields = MakeShared<FJsonObject>();
	for (FProperty* Property : EditableProperties(Style->GeneratedClass))
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			FString Value;
			UObject* Defaults = Style->GeneratedClass->GetDefaultObject();
			Property->ExportText_InContainer(Index, Value, Defaults, Defaults, Defaults, PPF_None);
			Values.Add(MakeShared<FJsonValueString>(Value));
		}
		TSharedRef<FJsonObject> Field = MakeShared<FJsonObject>();
		Field->SetStringField(TEXT("type"), PropertyType(Property));
		Field->SetArrayField(TEXT("values"), Values);
		Fields->SetObjectField(Property->GetName(), Field);
	}
	Payload->SetObjectField(TEXT("properties"), Fields);
	FString Text;
	FJsonSerializer::Serialize(Payload, TJsonWriterFactory<>::Create(&Text));
	if (!WidgetStyleClipboard::Write(Text))
	{
		Report = TEXT("System clipboard is unavailable.");
		return false;
	}
	Report = TEXT("Style properties copied to the system clipboard.");
	return true;
}

bool FWidgetThemeEditorService::CanPasteProperties(UWidgetTheme* Theme, FGameplayTag Tag)
{
	TSharedPtr<FJsonObject> Payload;
	FString Report;
	return !bBusy && ReadClipboard(Theme, Tag, Payload, Report);
}

bool FWidgetThemeEditorService::PasteProperties(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report)
{
	TSharedPtr<FJsonObject> Payload;
	if (!ReadClipboard(Theme, Tag, Payload, Report))
	{
		Report += TEXT("Copy compatible style properties first.");
		return false;
	}
	UBlueprint* Style = Blueprint(Theme, Tag);
	if (!Writable(Style->GetOutermost(), Report))
	{
		return false;
	}
	TGuardValue<bool> Busy(bBusy, true);
	FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "PasteProperties", "Paste style properties"));
	Theme->Modify();
	Style->Modify();
	UObject* Defaults = Style->GeneratedClass->GetDefaultObject();
	Defaults->Modify();
	FWidgetStylePropertyEditing Properties(Defaults);
	TMap<TSharedPtr<IPropertyHandle>, FString> Original;
	bool Good = true;
	for (const auto& Entry : Payload->GetObjectField(TEXT("properties"))->Values)
	{
		FProperty* Property = Defaults->GetClass()->FindPropertyByName(FName(*Entry.Key));
		TSharedPtr<IPropertyHandle> Handle = Properties.Find(Property);
		if (!Handle)
		{
			Good = false;
			break;
		}
		const TArray<TSharedPtr<FJsonValue>>& Values = Entry.Value->AsObject()->GetArrayField(TEXT("values"));
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			TSharedPtr<IPropertyHandle> Element = Property->ArrayDim > 1 ? Handle->GetChildHandle(Index) : Handle;
			FString Value;
			if (!Element)
			{
				Good = false;
				break;
			}
			Property->ExportText_InContainer(Index, Value, Defaults, Defaults, Defaults, PPF_None);
			Original.Add(Element, Value);
			if (Element->SetValueFromFormattedString(Values[Index]->AsString()) != FPropertyAccess::Success)
			{
				Good = false;
				break;
			}
		}
		if (!Good)
		{
			break;
		}
	}
	if (!Good || !DependencySafe(Defaults, Report))
	{
		for (const auto& Entry : Original)
		{
			Entry.Key->SetValueFromFormattedString(Entry.Value);
		}
		Transaction.Cancel();
		Report += TEXT("Properties could not be applied safely.");
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Style);
	FBlueprintEditorUtils::PostEditChangeBlueprintActors(Style);
	EWidgetStyleKind Type;
	Kind(Tag, Type);
	Theme->SetStyleClass(Tag, Type, Style->GeneratedClass);
	Style->MarkPackageDirty();
	Theme->MarkPackageDirty();
	FWidgetThemeResolver::NotifyThemeChanged();
	Report = TEXT("Properties pasted. Blueprint identity and parent retained. Save to persist.");
	return true;
}

bool FWidgetThemeEditorService::DuplicateThemeStyles(UWidgetTheme* Theme)
{
	if (!Theme || bBusy)
	{
		return false;
	}
	if (!ProjectContentMounts().Contains(MountRoot(Theme->GetOutermost()->GetName())))
	{
		return true;
	}
	TGuardValue<bool> Busy(bBusy, true);
	struct FCopyResult
	{
		UWidgetTheme* Theme;
		bool bComplete = false;
		~FCopyResult()
		{
			if (!bComplete)
			{
				for (FGameplayTag StyleTag : FWidgetThemeEditorService::Tags(Theme))
				{
					const FWidgetStyleBinding* Binding = Theme->Bindings.Find(StyleTag);
					if (Binding && (Binding->bManaged || Binding->SourceTheme == Theme))
					{
						EWidgetStyleKind Type;
						FWidgetThemeEditorService::Kind(StyleTag, Type);
						Theme->RemoveStyle(StyleTag, Type);
					}
				}
			}
		}
	} Result{Theme};
	FString Report;
	const FString DestinationRoot = ThemeRoot(Theme);
	TArray<UBlueprint*> Originals;
	for (const auto& Entry : Theme->Bindings)
	{
		if (Entry.Value.bManaged)
		{
			UBlueprint* Style = Blueprint(Theme, Entry.Key);
			if (!Style || !Style->GeneratedClass || Style->Status == BS_Error ||
				ForbiddenFrameworkReference(Style->ParentClass->GetPathName(), MountRoot(DestinationRoot)))
			{
				UE_LOG(LogTemp, Error, TEXT("Theme duplication has an invalid owned Blueprint: %s"), *Entry.Key.ToString());
				return false;
			}
			Originals.AddUnique(Style);
		}
	}
	Originals.Sort(
		[](const UBlueprint& Left, const UBlueprint& Right)
		{
			auto Depth = [](const UClass* Class)
			{
				int32 Count = 0;
				for (; Class; Class = Class->GetSuperClass())
				{
					++Count;
				}
				return Count;
			};
			const int32 LeftDepth = Depth(Left.GeneratedClass);
			const int32 RightDepth = Depth(Right.GeneratedClass);
			if (LeftDepth != RightDepth)
			{
				return LeftDepth < RightDepth;
			}
			return Left.GetPathName() < Right.GetPathName();
		});
	TMap<UBlueprint*, UBlueprint*> Copies;
	TMap<UClass*, UClass*> Classes;
	TMap<UObject*, UObject*> Replacements;
	TMap<UBlueprint*, FGuid> Identities;
	TArray<UBlueprint*> Created;
	bool Good = true;
	for (UBlueprint* Original : Originals)
	{
		FString Package;
		FGuid Identity;
		if (!NewPackage(DestinationRoot, Package, Identity, Report))
		{
			Good = false;
			break;
		}
		UPackage* CopyPackage = CreatePackage(*Package);
		CopyPackage->MarkAsFullyLoaded();
		UBlueprint* Copy = DuplicateObject<UBlueprint>(Original, CopyPackage, FName(*FPackageName::GetShortName(Package)));
		Copy->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		Created.Add(Copy);
		if (UClass** CopiedParent = Classes.Find(Original->ParentClass))
		{
			UBlueprintEditorLibrary::ReparentBlueprint(Copy, *CopiedParent);
		}
		FBlueprintEditorUtils::RefreshAllNodes(Copy);
		if (!Compile(Copy, Report))
		{
			Good = false;
			break;
		}
		Copies.Add(Original, Copy);
		Classes.Add(Original->GeneratedClass, Copy->GeneratedClass);
		Identities.Add(Copy, Identity);
		Replacements.Add(Original, Copy);
		Replacements.Add(Original->GeneratedClass, Copy->GeneratedClass);
		Replacements.Add(Original->GeneratedClass->GetDefaultObject(), Copy->GeneratedClass->GetDefaultObject());
		if (Original->SkeletonGeneratedClass && Copy->SkeletonGeneratedClass)
		{
			Replacements.Add(Original->SkeletonGeneratedClass, Copy->SkeletonGeneratedClass);
		}
	}
	if (Good)
	{
		for (UBlueprint* Copy : Created)
		{
			FArchiveReplaceObjectRef<UObject> Replace(Copy, Replacements,
													  EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
			FBlueprintEditorUtils::RefreshAllNodes(Copy);
			FBlueprintEditorUtils::MarkBlueprintAsModified(Copy);
			Good &= Compile(Copy, Report);
		}
	}
	if (!Good)
	{
		for (UBlueprint* Copy : Created)
		{
			Abandon(Copy);
		}
		UE_LOG(LogTemp, Error, TEXT("Theme style duplication failed; no private clones were retained. %s"), *Report);
		return false;
	}
	for (FGameplayTag Tag : Tags(Theme))
	{
		UBlueprint* Original = Blueprint(Theme, Tag);
		if (UBlueprint** Copy = Copies.Find(Original))
		{
			EWidgetStyleKind Type;
			Kind(Tag, Type);
			Theme->SetStyleClass(Tag, Type, (*Copy)->GeneratedClass);
			FWidgetStyleBinding& Binding = Theme->Bindings.FindChecked(Tag);
			Binding.AssetId = Identities.FindChecked(*Copy);
			if (!Binding.bManaged)
			{
				Binding.SourceTheme = Theme;
			}
		}
	}
	for (UBlueprint* Copy : Created)
	{
		FAssetRegistryModule::AssetCreated(Copy);
		Copy->MarkPackageDirty();
	}
	Theme->MarkPackageDirty();
	Result.bComplete = true;
	FWidgetThemeResolver::NotifyThemeChanged();
	return true;
}

namespace WidgetThemeEdit
{
	static FDelegateHandle ExtraDeleteHandle;
	static FDelegateHandle CanDeleteHandle;
	static FDelegateHandle PreDeleteHandle;
	static FDelegateHandle DeletedHandle;
	struct FPendingThemeRemoval
	{
		TWeakObjectPtr<UObject> Asset;
		FName Package;
		TArray<FGameplayTag> Tags;
	};
	static TArray<FPendingThemeRemoval> PendingRemovals;
	static UWidgetTheme* RemovalTheme = nullptr;
	static TArray<TWeakObjectPtr<UObject>> DeleteBatch;
	static uint64 DeleteBatchFrame = MAX_uint64;
	static TArray<FString> ReferenceTextDirectories()
	{
		TArray<FString> Out{FPaths::ProjectDir() / TEXT("Source"), FPaths::ProjectConfigDir()};
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
		{
			if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project)
			{
				Out.Add(Plugin->GetBaseDir() / TEXT("Source"));
				Out.Add(Plugin->GetBaseDir() / TEXT("Config"));
			}
		}
		return Out;
	}
	static bool PackageCouldContainTag(const FAssetData& Asset, const FString& Name)
	{
		if (Asset.IsAssetLoaded())
		{
			return true;
		}
		const FString Extension = Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName() ? FPackageName::GetMapPackageExtension()
																									: FPackageName::GetAssetPackageExtension();
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *FPackageName::LongPackageNameToFilename(Asset.PackageName.ToString(), Extension)))
		{
			return true;
		}
		const FTCHARToUTF8 Narrow(*Name);
		for (int32 Index = 0; Index < Bytes.Num(); ++Index)
		{
			if ((Index + Narrow.Length() <= Bytes.Num() && FMemory::Memcmp(Bytes.GetData() + Index, Narrow.Get(), Narrow.Length()) == 0) ||
				(Index + Name.Len() * sizeof(TCHAR) <= Bytes.Num() && FMemory::Memcmp(Bytes.GetData() + Index, *Name, Name.Len() * sizeof(TCHAR)) == 0))
			{
				return true;
			}
		}
		return false;
	}
	static bool ContainsTagText(const FString& Text, const FString& Tag)
	{
		FString Trial = Text;
		return RewriteTagText(Trial, FGameplayTag::RequestGameplayTag(FName(*Tag), false), WidgetStyleTags::Root(EWidgetStyleKind::Brush));
	}
	static bool HasTagUsage(FGameplayTag Tag, const TSet<FName>& DeletedPackages)
	{
		for (const FAssetData& Asset : ProjectAssets(false))
		{
			if (DeletedPackages.Contains(Asset.PackageName))
			{
				continue;
			}
			if (PackageCouldContainTag(Asset, Tag.ToString()) && !Asset.GetAsset())
			{
				return true;
			}
		}
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* Object = *It;
			if (!ProjectContentMounts().Contains(MountRoot(Object->GetOutermost()->GetName())) ||
				DeletedPackages.Contains(Object->GetOutermost()->GetFName()) || Object->HasAnyFlags(RF_Transient) || Object->IsA<UPackage>())
			{
				continue;
			}
			for (TFieldIterator<FProperty> Property(Object->GetClass()); Property; ++Property)
			{
				if (!Property->HasAnyPropertyFlags(CPF_Transient))
				{
					FString Text;
					Property->ExportText_InContainer(0, Text, Object, nullptr, nullptr, PPF_None);
					if (ContainsTagText(Text, Tag.ToString()))
					{
						return true;
					}
				}
			}
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && ContainsTagText(Pin->DefaultValue, Tag.ToString()))
					{
						return true;
					}
				}
			}
		}
		for (const FString& Directory : ReferenceTextDirectories())
		{
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false);
			for (const FString& File : Files)
			{
				FString Text;
				if (FFileHelper::LoadFileToString(Text, *File))
				{
					TArray<FString> Lines;
					Text.ParseIntoArrayLines(Lines, false);
					for (const FString& Line : Lines)
					{
						if (FPaths::GetExtension(File).Equals(TEXT("ini"), ESearchCase::IgnoreCase) && Line.Contains(TEXT("GameplayTagList=")))
						{
							continue;
						}
						if (ContainsTagText(Line, Tag.ToString()))
						{
							return true;
						}
					}
				}
			}
		}
		for (const FGameplayTagRedirect& Redirect : GetDefault<UGameplayTagsSettings>()->GameplayTagRedirects)
		{
			if (Redirect.NewTagName == Tag.GetTagName())
			{
				return true;
			}
		}
		return false;
	}
	static void CleanUnusedTag(FGameplayTag Tag, const TSet<FName>& DeletedPackages, FString& Report)
	{
		TSharedPtr<FGameplayTagNode> Node = UGameplayTagsManager::Get().FindTagNode(Tag);
		if (!Node || WidgetStyleTags::IsRoot(Tag) || HasTagUsage(Tag, DeletedPackages))
		{
			return;
		}
		for (FName SourceName : Node->GetAllSourceNames())
		{
			const FGameplayTagSource* Source = UGameplayTagsManager::Get().FindTagSource(SourceName);
			if (Source && Source->SourceType == EGameplayTagSourceType::Native)
			{
				return;
			}
		}
		if (!IGameplayTagsEditorModule::Get().DeleteTagFromINI(Node))
		{
			Report += TEXT("Unused Tag could not be removed from its definition: ") + Tag.ToString() + TEXT("\n");
			return;
		}
		Report += TEXT("Unused Tag removed: ") + Tag.ToString() + TEXT("\n");
	}
	static void AddOwnedStylesToDelete(const TArray<UObject*>& Assets, TSet<UObject*>& Extra)
	{
		for (UObject* Asset : Assets)
		{
			if (UWidgetTheme* Theme = Cast<UWidgetTheme>(Asset))
			{
				for (const auto& Entry : Theme->Bindings)
				{
					if (Entry.Value.bManaged)
					{
						if (UBlueprint* Style = FWidgetThemeEditorService::Blueprint(Theme, Entry.Key))
						{
							Extra.Add(Style);
						}
					}
				}
			}
		}
		TSet<UObject*> Destinations = Extra;
		Destinations.Append(Assets);
		for (UObject* Asset : Destinations.Array())
		{
			if (UBlueprint* Style = Cast<UBlueprint>(Asset))
			{
				Destinations.Add(Style->GeneratedClass);
				Destinations.Add(Style->SkeletonGeneratedClass);
			}
		}
		FARFilter Filter;
		Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;
		for (const FString& Root : ProjectContentMounts())
		{
			Filter.PackagePaths.Add(FName(*Root));
		}
		TArray<FAssetData> Redirectors;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter, Redirectors);
		bool Added;
		do
		{
			Added = false;
			for (const FAssetData& Asset : Redirectors)
			{
				UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset.GetAsset());
				if (Redirector && !Destinations.Contains(Redirector) && Redirector->DestinationObject && Destinations.Contains(Redirector->DestinationObject))
				{
					Extra.Add(Redirector);
					Destinations.Add(Redirector);
					Added = true;
				}
			}
		} while (Added);
	}
}

bool FWidgetThemeEditorService::CanDeleteAssets(const TArray<UObject*>& Assets, FString& Report)
{
	TSet<UObject*> Deletion(Assets);
	AddOwnedStylesToDelete(Assets, Deletion);
	TSet<FName> Packages;
	if (RemovalTheme)
	{
		Packages.Add(RemovalTheme->GetOutermost()->GetFName());
		for (const auto& Entry : RemovalTheme->Bindings)
		{
			if (Entry.Value.bManaged)
			{
				if (UBlueprint* Style = Blueprint(RemovalTheme, Entry.Key))
				{
					Packages.Add(Style->GetOutermost()->GetFName());
				}
			}
		}
	}
	TSet<UObject*> Objects;
	bool HasThemeStyles = false;
	for (UObject* Asset : Deletion)
	{
		Packages.Add(Asset->GetOutermost()->GetFName());
		Objects.Add(Asset);
		if (UWidgetTheme* Theme = Cast<UWidgetTheme>(Asset))
		{
			HasThemeStyles = true;
		}
		if (UBlueprint* Style = Cast<UBlueprint>(Asset); Style && Style->GeneratedClass && Style->GeneratedClass->IsChildOf(UObject::StaticClass()))
		{
			for (EWidgetStyleKind StyleKind : WidgetStyleTags::Kinds())
			{
				HasThemeStyles |= Style->GeneratedClass->IsChildOf(FWidgetThemeResolver::NativeBase(StyleKind));
			}
			Objects.Add(Style->GeneratedClass);
			Objects.Add(Style->GeneratedClass->GetDefaultObject());
		}
	}
	if (!HasThemeStyles)
	{
		return true;
	}
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	for (UObject* Asset : Deletion)
	{
		TArray<FName> Referencers;
		Registry.GetReferencers(Asset->GetOutermost()->GetFName(), Referencers, UE::AssetRegistry::EDependencyCategory::Package);
		for (FName Referencer : Referencers)
		{
			if (!Packages.Contains(Referencer))
			{
				Report += TEXT("Deletion is blocked by an external asset reference: ") + Referencer.ToString() + TEXT("\n");
				return false;
			}
		}
	}
	for (UWidgetTheme* Theme : Themes())
	{
		if (Deletion.Contains(Theme) || Theme == RemovalTheme)
		{
			continue;
		}
		for (FGameplayTag Tag : Tags(Theme))
		{
			if (Deletion.Contains(Blueprint(Theme, Tag)))
			{
				Report += TEXT("Style is still referenced by theme: ") + Theme->GetPathName() + TEXT("\n");
				return false;
			}
		}
	}
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Asset = *It;
		if (!Asset->IsAsset() || Asset->HasAnyFlags(RF_Transient) || Packages.Contains(Asset->GetOutermost()->GetFName()) ||
			!ProjectContentMounts().Contains(MountRoot(Asset->GetOutermost()->GetName())))
		{
			continue;
		}
		TArray<UObject*> References;
		FReferenceFinder Finder(References, Asset, false, true, true, true);
		Finder.FindReferences(Asset);
		for (UObject* Reference : References)
		{
			if (Objects.Contains(Reference))
			{
				Report += TEXT("Loaded asset still references the deletion set: ") + Asset->GetPathName() + TEXT("\n");
				return false;
			}
		}
	}
	return true;
}

void FWidgetThemeEditorService::RegisterAssetLifecycle()
{
	UWidgetTheme::DuplicateStyles.BindStatic(&DuplicateThemeStyles);
	ExtraDeleteHandle = FEditorDelegates::OnAddExtraObjectsToDelete.AddLambda(
		[](const TArray<UObject*>& Assets, TSet<UObject*>& Extra)
		{
			AddOwnedStylesToDelete(Assets, Extra);
			DeleteBatch.Reset();
			DeleteBatchFrame = GFrameCounter;
			for (UObject* Asset : Assets)
			{
				DeleteBatch.Add(Asset);
			}
			for (UObject* Asset : Extra)
			{
				DeleteBatch.AddUnique(Asset);
			}
		});
	CanDeleteHandle = FEditorDelegates::OnAssetsCanDelete.AddLambda(
		[](const TArray<UObject*>& Assets, FCanDeleteAssetResult& Result)
		{
			TArray<UObject*> CheckAssets = Assets;
			if (Assets.Num() == 1 && DeleteBatchFrame == GFrameCounter && DeleteBatch.Contains(Assets[0]))
			{
				for (const TWeakObjectPtr<UObject>& Asset : DeleteBatch)
				{
					if (Asset.IsValid())
					{
						CheckAssets.AddUnique(Asset.Get());
					}
				}
			}
			FString Report;
			if (!FWidgetThemeEditorService::CanDeleteAssets(CheckAssets, Report))
			{
				Result.Set(false);
				UE_LOG(LogTemp, Warning, TEXT("%s"), *Report);
			}
		});
	PreDeleteHandle = FEditorDelegates::OnAssetsPreDelete.AddLambda(
		[](const TArray<UObject*>& Assets)
		{
			PendingRemovals.Reset();
			for (UObject* Asset : Assets)
			{
				if (UWidgetTheme* Theme = Cast<UWidgetTheme>(Asset))
				{
					PendingRemovals.Add({Theme, Theme->GetOutermost()->GetFName(), FWidgetThemeEditorService::Tags(Theme)});
				}
				else
				{
					PendingRemovals.Add({Asset, Asset->GetOutermost()->GetFName(), {}});
				}
			}
		});
	DeletedHandle = FEditorDelegates::OnAssetsDeleted.AddLambda(
		[](const TArray<UClass*>& Classes)
		{
			TArray<FGameplayTag> RemovedTags;
			TSet<FName> DeletedPackages;
			for (const FPendingThemeRemoval& Pending : PendingRemovals)
			{
				if (!Pending.Asset.IsValid() || !Pending.Asset->HasAnyFlags(RF_Standalone))
				{
					RemovedTags.Append(Pending.Tags);
					DeletedPackages.Add(Pending.Package);
				}
			}
			UWidgetThemeSettings* Settings = GetMutableDefault<UWidgetThemeSettings>();
			bool ConfigChanged = false;
			if (DeletedPackages.Contains(Settings->DefaultTheme.ToSoftObjectPath().GetLongPackageFName()))
			{
				Settings->DefaultTheme.Reset();
				ConfigChanged = true;
			}
			ConfigChanged |= Settings->AvailableThemes.RemoveAll([&](const TSoftObjectPtr<UWidgetTheme>& Theme)
																 { return DeletedPackages.Contains(Theme.ToSoftObjectPath().GetLongPackageFName()); }) > 0;
			if (ConfigChanged && !Settings->TryUpdateDefaultConfigFile())
			{
				UE_LOG(LogTemp, Error, TEXT("Deleted theme paths could not be removed from the theme settings config."));
			}
			PendingRemovals.Reset();
			DeleteBatch.Reset();
			FString Report;
			for (FGameplayTag Tag : RemovedTags)
			{
				CleanUnusedTag(Tag, DeletedPackages, Report);
			}
			if (!Report.IsEmpty())
			{
				UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
			}
			FWidgetThemeResolver::InvalidateEditorDefault();
		});
}

void FWidgetThemeEditorService::UnregisterAssetLifecycle()
{
	UWidgetTheme::DuplicateStyles.Unbind();
	FEditorDelegates::OnAddExtraObjectsToDelete.Remove(ExtraDeleteHandle);
	FEditorDelegates::OnAssetsCanDelete.Remove(CanDeleteHandle);
	FEditorDelegates::OnAssetsPreDelete.Remove(PreDeleteHandle);
	FEditorDelegates::OnAssetsDeleted.Remove(DeletedHandle);
	PendingRemovals.Reset();
	DeleteBatch.Reset();
}

bool FWidgetThemeEditorService::CanRemove(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report)
{
	const FWidgetStyleBinding* Binding = Theme ? Theme->Bindings.Find(Tag) : nullptr;
	if (!Binding || WidgetStyleTags::IsRoot(Tag))
	{
		Report = TEXT("Select a configured non-root style to remove.");
		return false;
	}
	if (!Binding->bManaged)
	{
		Report = TEXT("Remove this theme's association. The shared source blueprint is retained.");
		return true;
	}
	UBlueprint* Style = Blueprint(Theme, Tag);
	if (!Style)
	{
		Report = TEXT("The owned style blueprint could not be resolved.");
		return false;
	}
	TArray<UObject*> Assets{Style};
	TGuardValue<UWidgetTheme*> IgnoreCurrentMapping(RemovalTheme, Theme);
	return CanDeleteAssets(Assets, Report);
}

bool FWidgetThemeEditorService::RemoveBinding(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report)
{
	if (!CanRemove(Theme, Tag, Report) || !Writable(Theme->GetOutermost(), Report))
	{
		return false;
	}
	const FWidgetStyleBinding Saved = Theme->Bindings.FindChecked(Tag);
	UBlueprint* Style = Blueprint(Theme, Tag);
	UClass* StyleClass = Style ? Style->GeneratedClass.Get() : nullptr;
	TArray<FGameplayTag> RemovedTags{Tag};
	if (Saved.bManaged)
	{
		for (FGameplayTag Other : Tags(Theme))
		{
			if (Other != Tag && Blueprint(Theme, Other) == Style)
			{
				RemovedTags.Add(Other);
			}
		}
	}
	TGuardValue<bool> Busy(bBusy, true);
	EWidgetStyleKind Type;
	Kind(Tag, Type);
	FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "RemoveStyle", "Remove style"));
	Theme->Modify();
	Theme->RemoveStyle(Tag, Type);
	TGuardValue<UWidgetTheme*> IgnoreCurrentMapping(RemovalTheme, Theme);
	if (Saved.bManaged && ObjectTools::ForceDeleteObjects({Style}, false) == 0)
	{
		Theme->SetStyleClass(Tag, Type, StyleClass);
		Theme->Bindings.Add(Tag, Saved);
		Transaction.Cancel();
		return false;
	}
	Theme->MarkPackageDirty();
	for (FGameplayTag RemovedTag : RemovedTags)
	{
		Kind(RemovedTag, Type);
		Theme->RemoveStyle(RemovedTag, Type);
		CleanUnusedTag(RemovedTag, {}, Report);
	}
	FWidgetThemeResolver::NotifyThemeChanged();
	Report += TEXT("Mapping and unreferenced owned style removed. Shared source retained.\n");
	return true;
}

bool FWidgetThemeEditorService::RenameTag(FGameplayTag Old, FGameplayTag New, FString& Report)
{
	EWidgetStyleKind OldKind;
	EWidgetStyleKind NewKind;
	if (!Kind(Old, OldKind) || !Kind(New, NewKind) || OldKind != NewKind || Old == New)
	{
		Report = TEXT("New Tag must be registered and retain the original style type root.");
		return false;
	}
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	const TArray<FAssetData> Assets = ProjectAssets(false);
	for (const FAssetData& A : Assets)
	{
		if (PackageCouldContainTag(A, Old.ToString()) && !A.GetAsset())
		{
			Report += TEXT("Cannot prove rename safe; unloadable asset: ") + A.GetObjectPathString() + TEXT("\n");
			return false;
		}
	}
	TArray<UObject*> Objects;
	TSet<UPackage*> Packages;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* O = *It;
		FString Package = O->GetOutermost()->GetName();
		if (!ProjectContentMounts().Contains(MountRoot(Package)))
		{
			continue;
		}
		if (O->HasAnyFlags(RF_Transient) || O->IsA<UPackage>())
		{
			continue;
		}
		if (!Writable(O->GetOutermost(), Report))
		{
			return false;
		}
		if (!RenameKeysSafe(O, Old, New, Report))
		{
			return false;
		}
		Objects.Add(O);
	}
	UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
	if (IFileManager::Get().IsReadOnly(*Settings->GetDefaultConfigFilename()))
	{
		Report = TEXT("Gameplay tag config is read-only.");
		return false;
	}
	for (const FGameplayTagRedirect& R : Settings->GameplayTagRedirects)
	{
		if (R.OldTagName == Old.GetTagName() && R.NewTagName != New.GetTagName())
		{
			Report = TEXT("Old Tag already has a different redirect. Resolve the conflict first.");
			return false;
		}
	}
	TMap<FString, FString> TextChanges;
	for (const FString& Directory : ReferenceTextDirectories())
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false);
		for (const FString& File : Files)
		{
			const FString Extension = FPaths::GetExtension(File).ToLower();
			if (Extension != TEXT("h") && Extension != TEXT("cpp") && Extension != TEXT("inl") && Extension != TEXT("ini") && Extension != TEXT("cs"))
			{
				continue;
			}
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *File))
			{
				Report += TEXT("Could not scan reference file: ") + File + TEXT("\n");
				return false;
			}
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines, false);
			bool Changed = false;
			for (FString& Line : Lines)
			{
				if (Line.Contains(TEXT("GameplayTagList=")) || Line.Contains(TEXT("GameplayTagRedirects=")))
				{
					continue;
				}
				Changed |= RewriteTagText(Line, Old, New);
			}
			if (Changed)
			{
				if (IFileManager::Get().IsReadOnly(*File))
				{
					Report += TEXT("Reference file is read-only: ") + File + TEXT("\n");
					return false;
				}
				TextChanges.Add(File, FString::Join(Lines, Text.Contains(TEXT("\r\n")) ? TEXT("\r\n") : TEXT("\n")) + TEXT("\n"));
			}
		}
	}
	FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "RenameTag", "Rename theme Tag with redirect"));
	for (UObject* O : Objects)
	{
		O->Modify();
		bool Changed = false;
		for (TFieldIterator<FProperty> P(O->GetClass()); P; ++P)
		{
			if (!P->HasAnyPropertyFlags(CPF_Transient))
			{
				for (int32 I = 0; I < P->ArrayDim; ++I)
				{
					Changed |= RewriteTag(*P, P->ContainerPtrToValuePtr<void>(O, I), Old, New);
				}
			}
		}
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(O))
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin)
				{
					Changed |= RewriteTagText(Pin->DefaultValue, Old, New);
				}
			}
		}
		if (Changed)
		{
			O->PostEditChange();
			O->MarkPackageDirty();
			Packages.Add(O->GetOutermost());
			ReferenceChangedPackages.Add(O->GetOutermost());
		}
	}
	bool Good = true;
	for (const FAssetData& A : Assets)
	{
		if (Packages.Contains(FindPackage(nullptr, *A.PackageName.ToString())))
		{
			if (auto* BP = Cast<UBlueprint>(A.GetAsset()))
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
				Good &= Compile(BP, Report);
			}
		}
	}
	if (!Good)
	{
		Report += TEXT("Rename is pending in memory; fix compile errors or Undo. No redirect config was written.\n");
		return false;
	}
	for (const auto& Change : TextChanges)
	{
		if (!FFileHelper::SaveStringToFile(Change.Value, *Change.Key, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Report += TEXT("Could not save rewritten reference file: ") + Change.Key + TEXT(". Changes remain pending.\n");
			return false;
		}
		Report += TEXT("Reference file updated: ") + Change.Key + TEXT("\n");
	}
	FString RedirectFilename = Settings->GetDefaultConfigFilename();
	if (TSharedPtr<FGameplayTagNode> Node = UGameplayTagsManager::Get().FindTagNode(Old))
	{
		for (FName SourceName : Node->GetAllSourceNames())
		{
			const FGameplayTagSource* Source = UGameplayTagsManager::Get().FindTagSource(SourceName);
			if (Source && Source->SourceTagList)
			{
				RedirectFilename = Source->SourceTagList->ConfigFileName;
				break;
			}
		}
	}
	if (!IGameplayTagsEditorModule::Get().RenameTagInINI(Old.ToString(), New.ToString(), false))
	{
		Report += TEXT("Gameplay Tag definition and redirect could not be saved.\n");
		return false;
	}
	FString WrittenConfig;
	const FString ExpectedRedirect = TEXT("OldTagName=\"") + Old.ToString() + TEXT("\",NewTagName=\"") + New.ToString() + TEXT("\"");
	if (!FFileHelper::LoadFileToString(WrittenConfig, *RedirectFilename) || !WrittenConfig.Contains(ExpectedRedirect))
	{
		Report += TEXT("Redirect config could not be written; asset changes remain pending in memory.\n");
		return false;
	}
	UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();
	Report += FString::Printf(TEXT("Renamed reflected references in %d packages. GUID names unchanged. Redirect config "
								   "is saved and must remain for old configs/saves; config writes are not undone by Ctrl+Z.\n"),
							  Packages.Num());
	FWidgetThemeResolver::NotifyThemeChanged();
	return true;
}
bool FWidgetThemeEditorService::Save(UWidgetTheme* T, FString& Report)
{
	TSet<UBlueprint*> Compiled;
	for (FGameplayTag Tag : Tags(T))
	{
		const FWidgetStyleBinding* Binding = T->Bindings.Find(Tag);
		UBlueprint* Style = Blueprint(T, Tag);
		if (Binding && Binding->bManaged && Style && Style->GetOutermost()->IsDirty() && !Compiled.Contains(Style))
		{
			if (!CompileStyle(T, Tag, Report))
			{
				return false;
			}
			Compiled.Add(Style);
		}
	}
	if (!Validate(T, Report))
	{
		return false;
	}
	FString RuntimeError;
	if (!FWidgetThemeResolver::Validate(T, RuntimeError))
	{
		Report += TEXT("Runtime theme validation failed: ") + RuntimeError + TEXT("\n");
		return false;
	}
	TArray<UPackage*> Packages;
	for (FGameplayTag Tag : Tags(T))
	{
		if (UBlueprint* BP = Blueprint(T, Tag))
		{
			const FWidgetStyleBinding* Binding = T->Bindings.Find(Tag);
			if (Binding && Binding->bManaged)
			{
				Packages.AddUnique(BP->GetOutermost());
			}
			else if (BP->GetOutermost()->IsDirty())
			{
				Report += TEXT("Shared style has unsaved external changes; save it in its owning editor first: ") + BP->GetPathName() + TEXT("\n");
				return false;
			}
		}
	}
	Packages.AddUnique(T->GetOutermost());
	for (UPackage* P : Packages)
	{
		if (!Writable(P, Report))
		{
			return false;
		}
	}
	bool Good = true;
	for (UPackage* P : Packages)
	{
		if (!P->IsDirty())
		{
			continue;
		}
		FString Filename = FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		bool Saved = UPackage::SavePackage(P, nullptr, *Filename, Args);
		Good &= Saved;
		Report += (Saved ? TEXT("Saved: ") : TEXT("FAILED: ")) + P->GetName() + TEXT("\n");
	}
	if (Good)
	{
		if (T->bRuntimeTheme)
		{
			auto* Packaging = GetMutableDefault<UProjectPackagingSettings>();
			FDirectoryPath Directory;
			Directory.Path = FPaths::GetPath(T->GetOutermost()->GetName());
			if (!Packaging->DirectoriesToAlwaysCook.ContainsByPredicate([&](const FDirectoryPath& Item) { return Item.Path == Directory.Path; }))
			{
				Packaging->DirectoriesToAlwaysCook.Add(Directory);
				if (!Packaging->TryUpdateDefaultConfigFile())
				{
					Report += TEXT("Could not persist the theme cook root.\n");
					return false;
				}
			}
		}
		auto* Settings = GetMutableDefault<UWidgetThemeSettings>();
		if (IFileManager::Get().IsReadOnly(*Settings->GetDefaultConfigFilename()))
		{
			Report += TEXT("Settings config is read-only; packages saved, config not saved.\n");
			return false;
		}
		if (!Settings->TryUpdateDefaultConfigFile())
		{
			Report += TEXT("Packages saved, but project settings config write failed.\n");
			return false;
		}
	}
	else
	{
		Report += TEXT("Partial disk save is not atomic. Fix listed failures and retry; do not revert unrelated packages.\n");
	}
	return Good;
}
bool FWidgetThemeEditorService::SetProjectDefault(UWidgetTheme* T, FString& Report)
{
	if (!T || !T->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
	{
		Report = TEXT("The runtime project default must belong to Game.");
		return false;
	}
	const bool bPreviouslyRuntime = T->bRuntimeTheme;
	T->bRuntimeTheme = true;
	T->MarkPackageDirty();
	if (!Validate(T, Report) || !Save(T, Report))
	{
		T->bRuntimeTheme = bPreviouslyRuntime;
		return false;
	}
	auto* S = GetMutableDefault<UWidgetThemeSettings>();
	if (IFileManager::Get().IsReadOnly(*S->GetDefaultConfigFilename()))
	{
		Report += TEXT("Project config is read-only.");
		return false;
	}
	S->Modify();
	const TSoftObjectPtr<UWidgetTheme> PreviousDefault = S->DefaultTheme;
	const TArray<TSoftObjectPtr<UWidgetTheme>> PreviousAvailable = S->AvailableThemes;
	S->DefaultTheme = T;
	S->AvailableThemes.AddUnique(T);
	if (!S->TryUpdateDefaultConfigFile())
	{
		S->DefaultTheme = PreviousDefault;
		S->AvailableThemes = PreviousAvailable;
		Report += TEXT("Could not write project default; previous selection retained.\n");
		return false;
	}
	FWidgetThemeResolver::InvalidateEditorDefault();
	Report += TEXT("Project default updated explicitly. Existing runtime game instances retain their active theme until a switch request.\n");
	return true;
}
bool FWidgetThemeEditorService::ExportInventory(const FString& Filename, FString& Report)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Styles, Themes, References;
	for (const FAssetData& Asset : ProjectAssets(false))
	{
		if (!Asset.GetAsset())
		{
			Report += TEXT("Inventory cannot load: ") + Asset.GetObjectPathString() + TEXT("\n");
			return false;
		}
	}
	for (const FAssetData& Asset : ProjectAssets(true))
	{
		UBlueprint* BP = Cast<UBlueprint>(Asset.GetAsset());
		if (!BP || !BP->GeneratedClass)
		{
			return false;
		}
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("asset"), BP->GetPathName());
		Item->SetStringField(TEXT("parent"), GetPathNameSafe(BP->ParentClass));
		TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
		for (FProperty* Property : EditableProperties(BP->GeneratedClass))
		{
			FString Value;
			Property->ExportText_InContainer(0, Value, BP->GeneratedClass->GetDefaultObject(), nullptr, nullptr, PPF_None);
			Values->SetStringField(Property->GetName(), Value);
		}
		Item->SetObjectField(TEXT("values"), Values);
		Styles.Add(MakeShared<FJsonValueObject>(Item));
	}
	for (UWidgetTheme* T : AllThemes())
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("asset"), T->GetPathName());
		TSharedRef<FJsonObject> Maps = MakeShared<FJsonObject>();
		for (FGameplayTag Tag : Tags(T))
		{
			EWidgetStyleKind Type;
			UClass* Class = Kind(Tag, Type) ? T->GetStyleClass(Tag, Type) : nullptr;
			if (!Class)
			{
				for (EWidgetStyleKind StyleKind : WidgetStyleTags::Kinds())
				{
					if (UClass* Candidate = T->GetStyleClass(Tag, StyleKind))
					{
						Class = Candidate;
						break;
					}
				}
			}
			Maps->SetStringField(Tag.ToString(), GetPathNameSafe(Class));
		}
		Item->SetObjectField(TEXT("maps"), Maps);
		Themes.Add(MakeShared<FJsonValueObject>(Item));
	}
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Object = *It;
		if (!ProjectContentMounts().Contains(MountRoot(Object->GetOutermost()->GetName())) || Object->HasAnyFlags(RF_Transient))
		{
			continue;
		}
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || !Pin->DefaultValue.Contains(TEXT("Style.")))
				{
					continue;
				}
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("object"), Node->GetPathName());
				Item->SetStringField(TEXT("property"), TEXT("Pin.") + Pin->PinName.ToString() + TEXT(".DefaultValue"));
				Item->SetStringField(TEXT("value"), Pin->DefaultValue);
				References.Add(MakeShared<FJsonValueObject>(Item));
			}
		}
		for (TFieldIterator<FProperty> Property(Object->GetClass()); Property; ++Property)
		{
			if (Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}
			FString Value;
			Property->ExportText_InContainer(0, Value, Object, nullptr, nullptr, PPF_None);
			if (!Value.Contains(TEXT("Style.")))
			{
				continue;
			}
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("object"), Object->GetPathName());
			Item->SetStringField(TEXT("property"), Property->GetName());
			Item->SetStringField(TEXT("value"), Value);
			References.Add(MakeShared<FJsonValueObject>(Item));
		}
	}
	Root->SetArrayField(TEXT("styles"), Styles);
	Root->SetArrayField(TEXT("themes"), Themes);
	Root->SetArrayField(TEXT("references"), References);
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	Report += FString::Printf(TEXT("Inventory: %d styles, %d themes, %d reflected reference fields\n"), Styles.Num(), Themes.Num(), References.Num());
	return FFileHelper::SaveStringToFile(Json, *Filename);
}
bool FWidgetThemeEditorService::PersistReferenceChanges(FString& Report)
{
	for (UPackage* Package : ReferenceChangedPackages)
	{
		if (!Writable(Package, Report))
		{
			return false;
		}
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString Extension = Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
		if (!UPackage::SavePackage(Package, nullptr, *FPackageName::LongPackageNameToFilename(Package->GetName(), Extension), Args))
		{
			return false;
		}
		Report += TEXT("Reference rewrite saved: ") + Package->GetName() + TEXT("\n");
	}
	ReferenceChangedPackages.Reset();
	return true;
}
bool FWidgetThemeEditorService::OrganizeAssets(UWidgetTheme* Theme, FString& Report)
{
	if (!Theme || !Validate(Theme, Report))
	{
		return false;
	}
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	TArray<FTopLevelAssetPath> Bases;
	for (EWidgetStyleKind Kind : WidgetStyleTags::Kinds())
	{
		Bases.Add(FWidgetThemeResolver::NativeBase(Kind)->GetClassPathName());
	}
	TSet<FTopLevelAssetPath> Derived;
	Registry.GetDerivedClassNames(Bases, {}, Derived);
	FARFilter Filter;
	for (const FString& Root : ProjectContentMounts())
	{
		Filter.PackagePaths.Add(FName(*Root));
	}
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	TArray<UObject*> ToMove{Theme};
	for (const FAssetData& Asset : Assets)
	{
		FString GeneratedClass;
		if (!Asset.GetTagValue(TEXT("GeneratedClass"), GeneratedClass))
		{
			continue;
		}
		const FTopLevelAssetPath ClassPath(FPackageName::ExportTextPathToObjectPath(GeneratedClass));
		if (!Derived.Contains(ClassPath))
		{
			continue;
		}
		UBlueprint* BlueprintAsset = Cast<UBlueprint>(Asset.GetAsset());
		if (!BlueprintAsset || !BlueprintAsset->GeneratedClass || BlueprintAsset->Status == BS_Error)
		{
			Report += TEXT("Unusable style Blueprint: ") + Asset.GetObjectPathString() + TEXT("\n");
			return false;
		}
		ToMove.AddUnique(BlueprintAsset);
	}
	TArray<FAssetRenameData> Renames;
	TSet<FString> Destinations;
	for (UObject* Asset : ToMove)
	{
		const FString SourcePackage = Asset->GetOutermost()->GetName();
		const bool bFramework = !SourcePackage.StartsWith(TEXT("/Game/"));
		const FString DestinationRoot = Asset->IsA<UBlueprint>() ? ThemeRoot(Asset) : MountRoot(SourcePackage) / TEXT("DataAssets/Theme");
		if (SourcePackage.StartsWith(DestinationRoot + TEXT("/")))
		{
			continue;
		}
		if (!Writable(Asset->GetOutermost(), Report) || (bFramework && !DependencySafe(Asset, Report, true)))
		{
			return false;
		}
		FString AssetName = Asset->GetName();
		if (Asset->IsA<UWidgetTheme>())
		{
			AssetName = SourcePackage.StartsWith(TEXT("/Game/")) ? TEXT("DA_WidgetTheme_Game") : TEXT("DA_WidgetTheme_") + MountRoot(SourcePackage).Mid(1);
		}
		const FString Destination = DestinationRoot / AssetName;
		if (Destinations.Contains(Destination) || FindPackage(nullptr, *Destination) || FPackageName::DoesPackageExist(Destination))
		{
			Report += TEXT("Destination collision; no assets moved: ") + Destination + TEXT("\n");
			return false;
		}
		Destinations.Add(Destination);
		Renames.Emplace(Asset, DestinationRoot, AssetName);
		Report += TEXT("Organize: ") + SourcePackage + TEXT(" -> ") + Destination + TEXT("\n");
	}
	if (Renames.IsEmpty())
	{
		Report += TEXT("Theme, catalog and existing styles already use their owner theme roots.\n");
		return true;
	}
	if (!RenameWithRedirectors(Renames, Report))
	{
		Report += TEXT("AssetTools reported a canceled or partial move. Inspect the listed paths before retrying.\n");
		return false;
	}
	FWidgetThemeResolver::InvalidateEditorDefault();
	Report += TEXT("AssetTools organization completed; old-path redirectors retained.\n");
	return Validate(Theme, Report);
}
