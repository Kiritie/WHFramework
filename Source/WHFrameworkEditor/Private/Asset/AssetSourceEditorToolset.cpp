#include "Asset/AssetSourceEditorToolset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorReimportHandler.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/AssetManager.h"
#include "Engine/CurveTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Hash/Blake3.h"
#include "IAssetTools.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UnrealType.h"

namespace
{
	FString SourceReply(const TSharedRef<FJsonObject>& Result, const FString& Error)
	{
		Result->SetBoolField(TEXT("success"), Error.IsEmpty());
		Result->SetStringField(TEXT("error"), Error);
		FString Json;
		FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
		return Json;
	}

	bool HasPIEWorld()
	{
		if (!GEditor) return false;
		for (const FWorldContext& Context : GEditor->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE) return true;
		}
		return false;
	}

	UAssetImportData* FindImportData(UObject* Asset)
	{
		const FObjectPropertyBase* Property = Asset ? FindFProperty<FObjectPropertyBase>(Asset->GetClass(), TEXT("AssetImportData")) : nullptr;
		return Property ? Cast<UAssetImportData>(Property->GetObjectPropertyValue_InContainer(Asset)) : nullptr;
	}

	ERichCurveInterpMode GetSimpleTableImportMode(const UCurveTable* Table)
	{
		if (!Table) return RCIM_None;
		if (Table->ImportCurveInterpMode != RCIM_None) return Table->ImportCurveInterpMode;
		if (Table->GetCurveTableMode() != ECurveTableMode::SimpleCurves || Table->GetSimpleCurveRowMap().IsEmpty()) return RCIM_None;
		const FSimpleCurve* FirstCurve = Table->GetSimpleCurveRowMap().CreateConstIterator().Value();
		return FirstCurve ? FirstCurve->GetKeyInterpMode() : RCIM_None;
	}

	FString DescribeSource(UObject* Asset, const TSharedRef<FJsonObject>& Result, bool bRequireRecordedHash = true, bool bAllowMissingSource = false)
	{
		if (!IsInGameThread() || !IsValid(Asset) || !Asset->GetOutermost() || !FindImportData(Asset))
		{
			return TEXT("A valid asset with import data is required on the game thread.");
		}
		const UAssetImportData* ImportData = FindImportData(Asset);
		const TArray<FAssetImportInfo::FSourceFile>& Sources = ImportData->GetSourceData().SourceFiles;
		Result->SetStringField(TEXT("asset"), Asset->GetPathName());
		Result->SetStringField(TEXT("package"), Asset->GetOutermost()->GetName());
		Result->SetBoolField(TEXT("dirty"), Asset->GetOutermost()->IsDirty());
		Result->SetNumberField(TEXT("sourceCount"), Sources.Num());
		if (Sources.IsEmpty() && bAllowMissingSource)
		{
			return FString();
		}
		if (Sources.Num() != 1 || (bRequireRecordedHash && !Sources[0].FileHash.IsValid()))
		{
			return TEXT("Exactly one source with a valid recorded MD5 is required.");
		}
		Result->SetStringField(TEXT("relativeSource"), Sources[0].RelativeFilename);
		Result->SetStringField(TEXT("resolvedSource"), ImportData->GetFirstFilename());
		Result->SetStringField(TEXT("recordedMD5"), Sources[0].FileHash.IsValid() ? LexToString(Sources[0].FileHash) : FString());
		Result->SetBoolField(TEXT("recordedMD5Valid"), Sources[0].FileHash.IsValid());
		Result->SetStringField(TEXT("recordedTimestamp"), Sources[0].Timestamp.ToIso8601());
		Result->SetStringField(TEXT("sourceLabel"), Sources[0].DisplayLabelName);
		Result->SetBoolField(TEXT("resolvedSourceExists"), FPaths::FileExists(ImportData->GetFirstFilename()));
		return FString();
	}
}

FString UAssetSourceEditorToolset::FixupRedirectorPackages(const TArray<FString>& PackageNames,
	const TArray<FString>& ExpectedReferencers, bool bApply)
{
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	if (!GEditor || HasPIEWorld() || PackageNames.IsEmpty() || ExpectedReferencers.IsEmpty())
	{
		return SourceReply(Result, TEXT("需要编辑器、非 PIE 状态及明确的重定向器和引用包清单。"));
	}
	TSet<FName> AllowedReferencers;
	for (const FString& Name : ExpectedReferencers)
	{
		if (!FPackageName::IsValidLongPackageName(Name) || AllowedReferencers.Contains(FName(*Name)))
			return SourceReply(Result, TEXT("引用包清单无效或重复：") + Name);
		AllowedReferencers.Add(FName(*Name));
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<UObjectRedirector*> Redirectors;
	TSet<FName> SeenPackages;
	TSet<FName> ActualReferencers;
	for (const FString& Name : PackageNames)
	{
		const FName PackageName(*Name);
		if (!FPackageName::IsValidLongPackageName(Name) || SeenPackages.Contains(PackageName))
			return SourceReply(Result, TEXT("重定向包清单无效或重复：") + Name);
		SeenPackages.Add(PackageName);
		UPackage* Package = FindPackage(nullptr, *Name);
		if (!Package) Package = LoadPackage(nullptr, *Name, LOAD_None);
		UObjectRedirector* Redirector = Package ? FindObject<UObjectRedirector>(Package, *FPackageName::GetShortName(Name)) : nullptr;
		if (!Redirector || !Redirector->DestinationObject || Package->IsDirty())
			return SourceReply(Result, TEXT("重定向器无效、目标缺失或包未保存：") + Name);
		TArray<FName> Referencers;
		Registry.GetReferencers(PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::All);
		Referencers.Remove(PackageName);
		for (FName Referencer : Referencers)
		{
			if (!AllowedReferencers.Contains(Referencer)) return SourceReply(Result, TEXT("发现未列出的引用包：") + Referencer.ToString());
			ActualReferencers.Add(Referencer);
		}
		Redirectors.Add(Redirector);
	}
	if (ActualReferencers.Num() != AllowedReferencers.Num()) return SourceReply(Result, TEXT("引用包清单与实际引用不一致。"));
	for (FName Name : AllowedReferencers)
	{
		UPackage* Package = FindPackage(nullptr, *Name.ToString());
		if (Package && Package->IsDirty()) return SourceReply(Result, TEXT("引用包存在未保存修改：") + Name.ToString());
	}
	Result->SetNumberField(TEXT("redirectors"), Redirectors.Num());
	Result->SetNumberField(TEXT("referencers"), ActualReferencers.Num());
	Result->SetBoolField(TEXT("applied"), false);
	if (bApply)
	{
		IAssetTools& Tools = FAssetToolsModule::GetModule().Get();
		Tools.FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);
		if (Tools.IsFixupReferencersInProgress()) return SourceReply(Result, TEXT("重定向器修复尚未结束。"));
		TArray<TSharedPtr<FJsonValue>> Remaining;
		for (const FString& Name : PackageNames)
			if (FPackageName::DoesPackageExist(Name)) Remaining.Add(MakeShared<FJsonValueString>(Name));
		Result->SetArrayField(TEXT("remaining"), Remaining);
		Result->SetBoolField(TEXT("applied"), true);
		if (!Remaining.IsEmpty()) return SourceReply(Result, TEXT("部分重定向包仍存在，需检查引用和保存结果。"));
	}
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::InspectPrimaryAssetPaths(const TArray<FString>& PrimaryIds)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const UAssetManager* Manager = UAssetManager::GetIfInitialized();
	if (!IsInGameThread() || !Manager || PrimaryIds.Num() > 512)
	{
		return SourceReply(Result, TEXT("An initialized asset manager and at most 512 IDs are required on the game thread."));
	}
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FString& Text : PrimaryIds)
	{
		const FPrimaryAssetId Id = FPrimaryAssetId::FromString(Text);
		const FSoftObjectPath Path = Id.IsValid() ? Manager->GetPrimaryAssetPath(Id) : FSoftObjectPath();
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), Text);
		Entry->SetBoolField(TEXT("validId"), Id.IsValid());
		Entry->SetBoolField(TEXT("registered"), Path.IsValid());
		Entry->SetStringField(TEXT("path"), Path.ToString());
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Result->SetArrayField(TEXT("assets"), Entries);
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::InspectAssetSource(UObject* Asset)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	return SourceReply(Result, DescribeSource(Asset, Result, false));
}

FString UAssetSourceEditorToolset::InspectTextureSourcePixels(UObject* Asset)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	UTexture2D* Texture = Cast<UTexture2D>(Asset);
	if (!IsInGameThread() || !IsValid(Texture) || !Texture->Source.IsValid() ||
		Texture->Source.GetNumBlocks() != 1 || Texture->Source.GetNumLayers() != 1 ||
		Texture->Source.GetNumMips() < 1 || Texture->Source.GetNumSlices() != 1)
	{
		return SourceReply(Result, TEXT("A valid single-block, single-layer, single-slice Texture2D source is required on the game thread."));
	}
	const int64 ExpectedBytes = Texture->Source.CalcMipSize(0, 0, 0);
	if (ExpectedBytes <= 0 || ExpectedBytes > 512LL * 1024 * 1024)
	{
		return SourceReply(Result, TEXT("Source mip size is invalid or exceeds 512 MiB."));
	}
	TArray64<uint8> Pixels;
	if (!Texture->Source.GetMipData(Pixels, 0, 0, 0) || Pixels.Num() != ExpectedBytes)
	{
		return SourceReply(Result, TEXT("Could not read the complete uncompressed source mip."));
	}
	Result->SetStringField(TEXT("asset"), Texture->GetPathName());
	Result->SetBoolField(TEXT("dirty"), Texture->GetOutermost()->IsDirty());
	Result->SetNumberField(TEXT("width"), Texture->Source.GetSizeX());
	Result->SetNumberField(TEXT("height"), Texture->Source.GetSizeY());
	Result->SetNumberField(TEXT("format"), static_cast<int32>(Texture->Source.GetFormat()));
	Result->SetNumberField(TEXT("gamma"), static_cast<int32>(Texture->Source.GetGammaSpace(0)));
	Result->SetNumberField(TEXT("mipCount"), Texture->Source.GetNumMips());
	Result->SetNumberField(TEXT("bytes"), Pixels.Num());
	Result->SetStringField(TEXT("blake3"), LexToString(FBlake3::HashBuffer(Pixels.GetData(), Pixels.Num())));
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::RelocateAssetSource(UObject* Asset, const FString& ExpectedPackage,
	const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, bool bApply)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const FString DescribeError = DescribeSource(Asset, Result);
	if (!DescribeError.IsEmpty()) return SourceReply(Result, DescribeError);
	UAssetImportData* ImportData = FindImportData(Asset);
	const FAssetImportInfo Before = ImportData->GetSourceData();
	const FAssetImportInfo::FSourceFile& Source = Before.SourceFiles[0];
	const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(NewSource);
	const FMD5Hash ActualHash = FMD5Hash::HashFile(*AbsoluteSource);
	if (ExpectedPackage.IsEmpty() || !Asset->GetOutermost()->GetName().Equals(ExpectedPackage, ESearchCase::CaseSensitive) ||
		!Source.RelativeFilename.Equals(ExpectedRelativeSource, ESearchCase::CaseSensitive) ||
		!LexToString(Source.FileHash).Equals(ExpectedMD5, ESearchCase::IgnoreCase) || !ActualHash.IsValid() ||
		ActualHash != Source.FileHash || !FPaths::FileExists(AbsoluteSource))
	{
		return SourceReply(Result, TEXT("Package, existing source metadata or replacement file hash does not match the expected values."));
	}
	Result->SetStringField(TEXT("newSource"), AbsoluteSource);
	Result->SetBoolField(TEXT("applied"), false);
	if (!bApply) return SourceReply(Result, FString());
	if (!GEditor || HasPIEWorld() || Asset->GetOutermost()->IsDirty())
	{
		return SourceReply(Result, TEXT("A clean asset package and a non-PIE editor are required before editing."));
	}
	if (FPaths::IsSamePath(ImportData->GetFirstFilename(), AbsoluteSource)) return SourceReply(Result, FString());
	const FScopedTransaction Transaction(NSLOCTEXT("WHFramework", "RelocateAssetSource", "Relocate asset import source"));
	Asset->Modify();
	ImportData->Modify();
	ImportData->UpdateFilenameOnly(AbsoluteSource);
	const FAssetImportInfo& After = ImportData->GetSourceData();
	if (After.SourceFiles.Num() != 1 || After.SourceFiles[0].FileHash != Source.FileHash ||
		After.SourceFiles[0].Timestamp != Source.Timestamp || After.SourceFiles[0].DisplayLabelName != Source.DisplayLabelName ||
		!FPaths::IsSamePath(ImportData->GetFirstFilename(), AbsoluteSource))
	{
		ImportData->SourceData = Before;
		return SourceReply(Result, TEXT("The rewritten source could not be verified; original metadata was restored in memory."));
	}
	Asset->MarkPackageDirty();
	Result->SetBoolField(TEXT("applied"), true);
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::ReimportTextureFromSource(UObject* Asset, const FString& ExpectedPackage,
	const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, const FString& NewMD5, bool bApply)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const FString DescribeError = DescribeSource(Asset, Result);
	if (!DescribeError.IsEmpty()) return SourceReply(Result, DescribeError);
	UTexture2D* Texture = Cast<UTexture2D>(Asset);
	UAssetImportData* ImportData = FindImportData(Asset);
	const FAssetImportInfo::FSourceFile& Source = ImportData->GetSourceData().SourceFiles[0];
	const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(NewSource);
	const FMD5Hash ActualHash = FMD5Hash::HashFile(*AbsoluteSource);
	if (!Texture || !GEditor || HasPIEWorld() || Asset->GetOutermost()->IsDirty() ||
		ExpectedPackage.IsEmpty() || !Asset->GetOutermost()->GetName().Equals(ExpectedPackage, ESearchCase::CaseSensitive) ||
		!Source.RelativeFilename.Equals(ExpectedRelativeSource, ESearchCase::CaseSensitive) ||
		!LexToString(Source.FileHash).Equals(ExpectedMD5, ESearchCase::IgnoreCase) ||
		!FPaths::GetExtension(AbsoluteSource).Equals(TEXT("png"), ESearchCase::IgnoreCase) ||
		!ActualHash.IsValid() || !LexToString(ActualHash).Equals(NewMD5, ESearchCase::IgnoreCase))
	{
		return SourceReply(Result, TEXT("Texture, editor state or source metadata does not match the expected values."));
	}
	Result->SetStringField(TEXT("newSource"), AbsoluteSource);
	Result->SetBoolField(TEXT("applied"), false);
	if (!bApply) return SourceReply(Result, FString());
	if (!FReimportManager::Instance()->Reimport(Texture, false, false, AbsoluteSource, nullptr, INDEX_NONE, true, true))
	{
		return SourceReply(Result, TEXT("Texture reimport failed; inspect the asset before saving."));
	}
	const FAssetImportInfo& After = ImportData->GetSourceData();
	if (After.SourceFiles.Num() != 1 || After.SourceFiles[0].FileHash != ActualHash ||
		!FPaths::IsSamePath(ImportData->GetFirstFilename(), AbsoluteSource) || FMD5Hash::HashFile(*AbsoluteSource) != ActualHash)
	{
		return SourceReply(Result, TEXT("Reimported source could not be verified; inspect the asset before saving."));
	}
	Result->SetNumberField(TEXT("width"), Texture->Source.GetSizeX());
	Result->SetNumberField(TEXT("height"), Texture->Source.GetSizeY());
	Result->SetBoolField(TEXT("applied"), true);
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::RebindEquivalentCurveTableSource(UObject* Asset, const FString& ExpectedPackage,
	const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, const FString& NewMD5, bool bApply)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const FString DescribeError = DescribeSource(Asset, Result, false, true);
	if (!DescribeError.IsEmpty()) return SourceReply(Result, DescribeError);
	UCurveTable* Table = Cast<UCurveTable>(Asset);
	UAssetImportData* ImportData = FindImportData(Asset);
	const FAssetImportInfo Before = ImportData->GetSourceData();
	const FAssetImportInfo::FSourceFile* Source = Before.SourceFiles.IsEmpty() ? nullptr : &Before.SourceFiles[0];
	const FString PreviousSource = Source ? Source->RelativeFilename : FString();
	const FString PreviousMD5 = Source && Source->FileHash.IsValid() ? LexToString(Source->FileHash) : FString();
	const FString SourceLabel = Source ? Source->DisplayLabelName : FString();
	const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(NewSource);
	const FMD5Hash ActualHash = FMD5Hash::HashFile(*AbsoluteSource);
	const ERichCurveInterpMode ImportMode = GetSimpleTableImportMode(Table);
	if (!Table || Table->GetClass() != UCurveTable::StaticClass() || Table->GetCurveTableMode() != ECurveTableMode::SimpleCurves ||
		(ImportMode != RCIM_Linear && ImportMode != RCIM_Constant) ||
		ExpectedPackage.IsEmpty() || !Asset->GetOutermost()->GetName().Equals(ExpectedPackage, ESearchCase::CaseSensitive) ||
		!PreviousSource.Equals(ExpectedRelativeSource, ESearchCase::CaseSensitive) ||
		!PreviousMD5.Equals(ExpectedMD5, ESearchCase::IgnoreCase) ||
		!FPaths::GetExtension(AbsoluteSource).Equals(TEXT("csv"), ESearchCase::IgnoreCase) ||
		!ActualHash.IsValid() || !LexToString(ActualHash).Equals(NewMD5, ESearchCase::IgnoreCase))
	{
		return SourceReply(Result, TEXT("Curve type, interpolation, package or source metadata does not match the expected values."));
	}
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *AbsoluteSource) || FMD5Hash::HashFile(*AbsoluteSource) != ActualHash)
	{
		return SourceReply(Result, TEXT("The replacement CSV could not be read consistently."));
	}
	UCurveTable* Candidate = NewObject<UCurveTable>(GetTransientPackage());
	const TArray<FString> Problems = Candidate->CreateTableFromCSVString(Csv, ImportMode);
	if (!Problems.IsEmpty() || Candidate->GetCurveTableMode() != ECurveTableMode::SimpleCurves ||
		Candidate->GetRowMap().Num() != Table->GetRowMap().Num() || Table->GetRowMap().IsEmpty())
	{
		return SourceReply(Result, TEXT("The replacement CSV is invalid or has different curve rows."));
	}
	int32 KeyCount = 0;
	for (const TPair<FName, FSimpleCurve*>& Entry : Table->GetSimpleCurveRowMap())
	{
		const FSimpleCurve* const* CandidateCurve = Candidate->GetSimpleCurveRowMap().Find(Entry.Key);
		const FSimpleCurve* Existing = Entry.Value;
		if (!CandidateCurve || !Existing || !*CandidateCurve ||
			Existing->GetKeyInterpMode() != (*CandidateCurve)->GetKeyInterpMode() ||
			Existing->GetDefaultValue() != (*CandidateCurve)->GetDefaultValue() ||
			Existing->PreInfinityExtrap != (*CandidateCurve)->PreInfinityExtrap ||
			Existing->PostInfinityExtrap != (*CandidateCurve)->PostInfinityExtrap ||
			Existing->GetConstRefOfKeys() != (*CandidateCurve)->GetConstRefOfKeys())
		{
			return SourceReply(Result, FString::Printf(TEXT("Replacement CSV differs from curve row %s."), *Entry.Key.ToString()));
		}
		KeyCount += Existing->GetConstRefOfKeys().Num();
	}
	Result->SetStringField(TEXT("newSource"), AbsoluteSource);
	Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
	Result->SetNumberField(TEXT("keyCount"), KeyCount);
	Result->SetBoolField(TEXT("fillsMissingImportMode"), Table->ImportCurveInterpMode == RCIM_None);
	Result->SetBoolField(TEXT("applied"), false);
	if (!bApply) return SourceReply(Result, FString());
	if (!GEditor || HasPIEWorld() || Asset->GetOutermost()->IsDirty() || FMD5Hash::HashFile(*AbsoluteSource) != ActualHash)
	{
		return SourceReply(Result, TEXT("A clean asset package, stable source and a non-PIE editor are required before editing."));
	}
	const FScopedTransaction Transaction(NSLOCTEXT("WHFramework", "RebindEquivalentCurveTableSource", "Rebind equivalent curve table source"));
	Asset->Modify();
	ImportData->Modify();
	const TEnumAsByte<ERichCurveInterpMode> PreviousImportMode = Table->ImportCurveInterpMode;
	Table->ImportCurveInterpMode = ImportMode;
	ImportData->AddFileName(AbsoluteSource, 0, SourceLabel);
	const FAssetImportInfo& After = ImportData->GetSourceData();
	if (After.SourceFiles.Num() != 1 || After.SourceFiles[0].FileHash != ActualHash ||
		After.SourceFiles[0].DisplayLabelName != SourceLabel ||
		!FPaths::IsSamePath(ImportData->GetFirstFilename(), AbsoluteSource))
	{
		ImportData->SourceData = Before;
		Table->ImportCurveInterpMode = PreviousImportMode;
		return SourceReply(Result, TEXT("The updated source could not be verified; original metadata was restored in memory."));
	}
	Asset->MarkPackageDirty();
	Result->SetBoolField(TEXT("applied"), true);
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::ReimportEquivalentCurveTable(UObject* Asset, const FString& ExpectedPackage,
	const FString& ExpectedRelativeSource, const FString& ExpectedMD5, bool bApply)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const FString DescribeError = DescribeSource(Asset, Result);
	if (!DescribeError.IsEmpty()) return SourceReply(Result, DescribeError);
	UCurveTable* Table = Cast<UCurveTable>(Asset);
	UAssetImportData* ImportData = FindImportData(Asset);
	if (!Table || !GEditor || HasPIEWorld() || Table->GetOutermost()->IsDirty() ||
		(Table->ImportCurveInterpMode != RCIM_Linear && Table->ImportCurveInterpMode != RCIM_Constant))
	{
		return SourceReply(Result, TEXT("A clean simple curve table with explicit import interpolation and a non-PIE editor are required."));
	}
	const FString Source = ImportData->GetFirstFilename();
	const FString Preflight = RebindEquivalentCurveTableSource(Asset, ExpectedPackage, ExpectedRelativeSource,
		ExpectedMD5, Source, ExpectedMD5, false);
	TSharedPtr<FJsonObject> Comparison;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Preflight), Comparison) ||
		!Comparison.IsValid() || !Comparison->GetBoolField(TEXT("success")))
	{
		Result->SetStringField(TEXT("comparison"), Preflight);
		return SourceReply(Result, TEXT("Source and existing curve table are not equivalent."));
	}
	Result->SetNumberField(TEXT("rowCount"), Comparison->GetNumberField(TEXT("rowCount")));
	Result->SetNumberField(TEXT("keyCount"), Comparison->GetNumberField(TEXT("keyCount")));
	Result->SetBoolField(TEXT("applied"), false);
	if (!bApply) return SourceReply(Result, FString());
	if (!FReimportManager::Instance()->Reimport(Table, false, false, Source, nullptr, INDEX_NONE, true, true))
	{
		return SourceReply(Result, TEXT("Curve table reimport failed; inspect the asset before saving."));
	}
	const FString After = RebindEquivalentCurveTableSource(Asset, ExpectedPackage, ExpectedRelativeSource,
		ExpectedMD5, Source, ExpectedMD5, false);
	TSharedPtr<FJsonObject> AfterComparison;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(After), AfterComparison) ||
		!AfterComparison.IsValid() || !AfterComparison->GetBoolField(TEXT("success")))
	{
		Result->SetStringField(TEXT("comparison"), After);
		return SourceReply(Result, TEXT("Reimported curve table failed equivalence checks; inspect it before saving."));
	}
	Result->SetBoolField(TEXT("applied"), true);
	return SourceReply(Result, FString());
}

FString UAssetSourceEditorToolset::UpdateSimpleCurveTableFromSource(UObject* Asset, const FString& ExpectedPackage,
	const FString& ExpectedRelativeSource, const FString& ExpectedMD5, const FString& NewSource, const FString& NewMD5, bool bAllowNewRows, bool bApply)
{
	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	const FString DescribeError = DescribeSource(Asset, Result, false);
	if (!DescribeError.IsEmpty()) return SourceReply(Result, DescribeError);
	UCurveTable* Table = Cast<UCurveTable>(Asset);
	UAssetImportData* ImportData = FindImportData(Asset);
	const FAssetImportInfo Before = ImportData->GetSourceData();
	const FAssetImportInfo::FSourceFile& Source = Before.SourceFiles[0];
	const FString AbsoluteSource = FPaths::ConvertRelativePathToFull(NewSource);
	const FMD5Hash ActualHash = FMD5Hash::HashFile(*AbsoluteSource);
	const ERichCurveInterpMode ImportMode = GetSimpleTableImportMode(Table);
	if (!Table || Table->GetClass() != UCurveTable::StaticClass() || Table->GetCurveTableMode() != ECurveTableMode::SimpleCurves ||
		(ImportMode != RCIM_Linear && ImportMode != RCIM_Constant) ||
		ExpectedPackage.IsEmpty() || !Asset->GetOutermost()->GetName().Equals(ExpectedPackage, ESearchCase::CaseSensitive) ||
		!Source.RelativeFilename.Equals(ExpectedRelativeSource, ESearchCase::CaseSensitive) ||
		!(Source.FileHash.IsValid() ? LexToString(Source.FileHash) : FString()).Equals(ExpectedMD5, ESearchCase::IgnoreCase) ||
		!FPaths::GetExtension(AbsoluteSource).Equals(TEXT("csv"), ESearchCase::IgnoreCase) ||
		!ActualHash.IsValid() || !LexToString(ActualHash).Equals(NewMD5, ESearchCase::IgnoreCase))
	{
		return SourceReply(Result, TEXT("Curve type, interpolation, package or source metadata does not match the expected values."));
	}
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *AbsoluteSource) || FMD5Hash::HashFile(*AbsoluteSource) != ActualHash)
	{
		return SourceReply(Result, TEXT("The source CSV could not be read consistently."));
	}
	UCurveTable* Candidate = NewObject<UCurveTable>(GetTransientPackage());
	const TArray<FString> Problems = Candidate->CreateTableFromCSVString(Csv, ImportMode);
	if (!Problems.IsEmpty() || Candidate->GetCurveTableMode() != ECurveTableMode::SimpleCurves ||
		Candidate->GetRowMap().Num() < Table->GetRowMap().Num() || Table->GetRowMap().IsEmpty() ||
		(!bAllowNewRows && Candidate->GetRowMap().Num() != Table->GetRowMap().Num()))
	{
		return SourceReply(Result, TEXT("The source CSV is invalid or has different curve rows."));
	}
	int32 KeyCount = 0;
	int32 ChangedRows = 0;
	TMap<FName, TArray<FSimpleCurveKey>> PreviousKeys;
	TArray<FName> NewRows;
	for (const TPair<FName, FSimpleCurve*>& Entry : Table->GetSimpleCurveRowMap())
	{
		const FSimpleCurve* const* CandidateCurve = Candidate->GetSimpleCurveRowMap().Find(Entry.Key);
		const FSimpleCurve* Existing = Entry.Value;
		if (!CandidateCurve || !Existing || !*CandidateCurve ||
			Existing->GetKeyInterpMode() != (*CandidateCurve)->GetKeyInterpMode() ||
			Existing->GetDefaultValue() != (*CandidateCurve)->GetDefaultValue() ||
			Existing->PreInfinityExtrap != (*CandidateCurve)->PreInfinityExtrap ||
			Existing->PostInfinityExtrap != (*CandidateCurve)->PostInfinityExtrap)
		{
			if (CandidateCurve && Existing && *CandidateCurve)
			{
				Result->SetNumberField(TEXT("existingInterp"), static_cast<int32>(Existing->GetKeyInterpMode()));
				Result->SetNumberField(TEXT("candidateInterp"), static_cast<int32>((*CandidateCurve)->GetKeyInterpMode()));
				Result->SetNumberField(TEXT("existingDefault"), Existing->GetDefaultValue());
				Result->SetNumberField(TEXT("candidateDefault"), (*CandidateCurve)->GetDefaultValue());
				Result->SetNumberField(TEXT("existingPreExtrap"), static_cast<int32>(Existing->PreInfinityExtrap.GetValue()));
				Result->SetNumberField(TEXT("candidatePreExtrap"), static_cast<int32>((*CandidateCurve)->PreInfinityExtrap.GetValue()));
				Result->SetNumberField(TEXT("existingPostExtrap"), static_cast<int32>(Existing->PostInfinityExtrap.GetValue()));
				Result->SetNumberField(TEXT("candidatePostExtrap"), static_cast<int32>((*CandidateCurve)->PostInfinityExtrap.GetValue()));
			}
			return SourceReply(Result, FString::Printf(TEXT("Source CSV changes curve settings or row %s is missing."), *Entry.Key.ToString()));
		}
		PreviousKeys.Add(Entry.Key, Existing->GetCopyOfKeys());
		ChangedRows += Existing->GetConstRefOfKeys() != (*CandidateCurve)->GetConstRefOfKeys() ? 1 : 0;
		KeyCount += (*CandidateCurve)->GetConstRefOfKeys().Num();
	}
	for (const TPair<FName, FSimpleCurve*>& Entry : Candidate->GetSimpleCurveRowMap())
	{
		if (!Table->GetSimpleCurveRowMap().Contains(Entry.Key))
		{
			NewRows.Add(Entry.Key);
			KeyCount += Entry.Value->GetConstRefOfKeys().Num();
		}
	}
	Result->SetNumberField(TEXT("rowCount"), Table->GetRowMap().Num());
	Result->SetNumberField(TEXT("keyCount"), KeyCount);
	Result->SetNumberField(TEXT("changedRows"), ChangedRows);
	Result->SetNumberField(TEXT("newRows"), NewRows.Num());
	Result->SetBoolField(TEXT("fillsMissingImportMode"), Table->ImportCurveInterpMode == RCIM_None);
	Result->SetBoolField(TEXT("applied"), false);
	if (!bApply) return SourceReply(Result, FString());
	if (!GEditor || HasPIEWorld() || Asset->GetOutermost()->IsDirty() || FMD5Hash::HashFile(*AbsoluteSource) != ActualHash)
	{
		return SourceReply(Result, TEXT("A clean asset package, stable source and a non-PIE editor are required before editing."));
	}
	const FScopedTransaction Transaction(NSLOCTEXT("WHFramework", "UpdateSimpleCurveTableFromSource", "Update curve table from source"));
	Table->Modify();
	ImportData->Modify();
	const TEnumAsByte<ERichCurveInterpMode> PreviousImportMode = Table->ImportCurveInterpMode;
	Table->ImportCurveInterpMode = ImportMode;
	for (const TPair<FName, FSimpleCurve*>& Entry : Table->GetSimpleCurveRowMap())
	{
		Entry.Value->SetKeys(Candidate->GetSimpleCurveRowMap().FindChecked(Entry.Key)->GetConstRefOfKeys());
	}
	for (const FName RowName : NewRows)
	{
		const FSimpleCurve* CandidateCurve = Candidate->GetSimpleCurveRowMap().FindChecked(RowName);
		FSimpleCurve& Added = Table->AddSimpleCurve(RowName);
		Added.SetKeyInterpMode(CandidateCurve->GetKeyInterpMode());
		Added.SetKeys(CandidateCurve->GetConstRefOfKeys());
	}
	UCurveTable::InvalidateAllCachedCurves();
	ImportData->AddFileName(AbsoluteSource, 0, Source.DisplayLabelName);
	const FAssetImportInfo& After = ImportData->GetSourceData();
	if (After.SourceFiles.Num() != 1 || After.SourceFiles[0].FileHash != ActualHash ||
		After.SourceFiles[0].DisplayLabelName != Source.DisplayLabelName ||
		!FPaths::IsSamePath(ImportData->GetFirstFilename(), AbsoluteSource))
	{
		for (const FName RowName : NewRows) Table->RemoveRow(RowName);
		for (const TPair<FName, FSimpleCurve*>& Entry : Table->GetSimpleCurveRowMap())
		{
			Entry.Value->SetKeys(PreviousKeys.FindChecked(Entry.Key));
		}
		UCurveTable::InvalidateAllCachedCurves();
		ImportData->SourceData = Before;
		Table->ImportCurveInterpMode = PreviousImportMode;
		return SourceReply(Result, TEXT("Source metadata verification failed; original keys and metadata were restored in memory."));
	}
	Table->MarkPackageDirty();
	Table->OnCurveTableChanged().Broadcast();
	Result->SetBoolField(TEXT("applied"), true);
	return SourceReply(Result, FString());
}
