#include "Voxel/VoxelModuleCustomization.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelBuiltinFeatures.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Voxel/VoxelGenerationCompiler.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "VoxelModuleCustomization"

TSharedRef<IDetailCustomization> FVoxelModuleCustomization::MakeInstance()
{
	return MakeShared<FVoxelModuleCustomization>();
}

void FVoxelModuleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetSelectedObjects();
	if (SelectedObjects.Num() != 1)
	{
		return;
	}

	const TWeakObjectPtr<UVoxelModule> Module = Cast<UVoxelModule>(SelectedObjects[0].Get());
	if (!Module.IsValid())
	{
		return;
	}

	DetailBuilder.EditCategory(TEXT("Voxel|Generation"))
		.AddCustomRow(LOCTEXT("BakeGenerationRecipeFilter", "Bake Generation Recipe"))
		.WholeRowContent()
		[
			SNew(SButton)
			.Text(LOCTEXT("BakeGenerationRecipe", "Bake Generation Recipe"))
			.ToolTipText(LOCTEXT("BakeGenerationRecipeTooltip", "Compile and save the World Generation Profile selected on this Voxel Module."))
			.IsEnabled_Lambda([Module]() { return Module.IsValid() && (!GEditor || !GEditor->PlayWorld); })
			.OnClicked_Lambda([Module]()
			{
				if (!Module.IsValid() || (GEditor && GEditor->PlayWorld))
				{
					return FReply::Handled();
				}

				UVoxelWorldGenerationProfile* Profile = Module->GetWorldGenerationProfileAsset().LoadSynchronous();
				FString Error;
				if (!Profile)
				{
					Error = TEXT("Voxel Module has no World Generation Profile");
				}
				else if (VoxelBuiltinFeatures::Register(Error))
				{
					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
					IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
					AssetRegistry.SearchAllAssets(true);

					FARFilter Filter;
					Filter.ClassPaths.Add(UVoxelData::StaticClass()->GetClassPathName());
					Filter.bRecursiveClasses = true;
					Filter.PackagePaths.Add(TEXT("/WHFramework/Voxel/DataAssets"));
					Filter.PackagePaths.Add(TEXT("/Game/DataAssets/Voxel"));
					Filter.bRecursivePaths = true;

					TArray<FAssetData> Found;
					AssetRegistry.GetAssets(Filter, Found);
					TArray<UVoxelData*> Blocks;
					for (const FAssetData& Asset : Found)
					{
						if (UVoxelData* Block = Cast<UVoxelData>(Asset.GetAsset()))
						{
							Blocks.Add(Block);
						}
					}

					FVoxelRegistry Registry;
					if (Registry.Build(Blocks, false, Error))
					{
						const FScopedTransaction Transaction(LOCTEXT("BakeGenerationRecipeTransaction", "Bake Voxel Generation Recipe"));
						if (FVoxelGenerationCompiler::Compile(*Profile, *Registry.GetSnapshot(), Error))
						{
							FVoxelEditorAssetIO::Save(Profile, Error);
						}
					}
				}

				if (!Error.IsEmpty())
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
					UE_LOG(LogTemp, Error, TEXT("Voxel generation recipe bake failed: %s"), *Error);
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("Voxel generation recipe baked: %s cell=%d"),
						*Profile->GetPathName(), Profile->BakedCellCentimeters);
				}
				return FReply::Handled();
			})
		];
}

#undef LOCTEXT_NAMESPACE
