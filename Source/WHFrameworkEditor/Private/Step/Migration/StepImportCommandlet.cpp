#include "Step/Migration/StepImportCommandlet.h"

#include "Step/Base/StepAsset.h"
#include "Step/Graph/StepGraphOperations.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UStepImportCommandlet::UStepImportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UStepImportCommandlet::Main(const FString& Params)
{
	FString Input;
	FString AssetPath;
	FString RootPolicy;
	if(!FParse::Value(*Params, TEXT("In="), Input) || !FParse::Value(*Params, TEXT("Asset="), AssetPath) || !FParse::Value(*Params, TEXT("RootPolicy="), RootPolicy) || RootPolicy != TEXT("All"))
	{
		UE_LOG(LogTemp, Error, TEXT("需要 -In=<JSON> -Asset=/Game/.../DA.DA -RootPolicy=All"));
		return 1;
	}
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if(!FFileHelper::LoadFileToString(Text, *Input) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return 2;
	}
	FString Format;
	const TArray<TSharedPtr<FJsonValue>>* SourceNodes = nullptr;
	if(!Root->TryGetStringField(TEXT("Format"), Format) || Format != TEXT("WHFramework.StepNodes.1") || !Root->TryGetArrayField(TEXT("Nodes"), SourceNodes))
	{
		return 2;
	}
	UStepAsset* Asset = LoadObject<UStepAsset>(nullptr, *AssetPath);
	if(!Asset || !Asset->Steps.IsEmpty() || !Asset->Links.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("目标必须是备份后的旧源资产或空的新 StepAsset；不覆盖已有新图。"));
		return 3;
	}
	FString SourceAsset;
	if(!Root->TryGetStringField(TEXT("SourceAsset"), SourceAsset) || SourceAsset != Asset->GetPathName())
	{
		UE_LOG(LogTemp, Error, TEXT("导出源路径与导入目标不一致，拒绝改变稳定资产身份。"));
		return 4;
	}
	const TArray<UStepBase*> Imported = SourceNodes->IsEmpty() ? TArray<UStepBase*>() : FStepGraphOperations::ImportSteps(Asset, Text, FVector2D::ZeroVector, true, true);
	if(Imported.Num() != Root->GetArrayField(TEXT("Nodes")).Num())
	{
		return 5;
	}
	FString DisplayName;
	if(Root->TryGetStringField(TEXT("DisplayName"), DisplayName))
	{
		FTextStringHelper::ReadFromBuffer(*DisplayName, Asset->DisplayName);
	}
	Root->TryGetBoolField(TEXT("AutoStartFirst"), Asset->bAutoStartFirst);
	TArray<FText> Errors;
	if(!Asset->Compile(Errors))
	{
		return 6;
	}
	FString LegacyFirst;
	if(Root->TryGetStringField(TEXT("LegacyFirstGUID"), LegacyFirst) && !LegacyFirst.IsEmpty() && (Asset->CompiledGraph.RootGUIDs.IsEmpty() || LegacyFirst != Asset->CompiledGraph.RootGUIDs[0]))
	{
		UE_LOG(LogTemp, Warning, TEXT("旧 FirstStep=%s 与新图第一个根节点不同；Start 采用全部原根，需在图中显式重接。"), *LegacyFirst);
	}
	UPackage* Package = Asset->GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs) ? 0 : 7;
}
