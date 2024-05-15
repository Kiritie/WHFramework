// Fill out your copyright notice in the Description page of Project Settings.


#include "WHFrameworkEditorStatics.h"

#include "AssetToolsModule.h"
#include "Common/Blueprint/Slate/SCreateBlueprintAssetDialog.h"

UObject* UWHFrameworkEditorStatics::CreateAssetWithDialog(TSubclassOf<UFactory> InFactoryClass)
{
	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), InFactoryClass);

	const FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	return AssetToolsModule.Get().CreateAssetWithDialog(Factory->GetSupportedClass(), Factory);
}

UBlueprint* UWHFrameworkEditorStatics::CreateBlueprintAssetWithDialog(TSubclassOf<UBlueprintFactoryBase> InFactoryClass, bool bAutoOpenAsset)
{
	const TSharedRef<SCreateBlueprintAssetDialog> CreateBlueprintAssetDialog = SNew(SCreateBlueprintAssetDialog).BlueprintFactoryClass(InFactoryClass).IsAutoOpenAsset(bAutoOpenAsset);
	
	CreateBlueprintAssetDialog->ShowModal();

	return CreateBlueprintAssetDialog->GetBlueprintAsset();
}
