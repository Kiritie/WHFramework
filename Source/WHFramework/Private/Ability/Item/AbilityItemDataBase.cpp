#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModule.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::None);

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

void UAbilityItemDataBase::SetIconByTexture_Implementation(UTexture* InTexture, FVector2D InSize, int32 InIndex)
{
	if(!AAbilityModule::Get()->GetItemIconSourceMat()) return;
	
	if(const auto IconMat = UMaterialInstanceDynamic::Create(AAbilityModule::Get()->GetItemIconSourceMat(), nullptr))
	{
		Icon = IconMat;
		IconMat->SetTextureParameterValue(FName("Texture"), InTexture);
		IconMat->SetScalarParameterValue(FName("SizeX"), InSize.X);
		IconMat->SetScalarParameterValue(FName("SizeY"), InSize.Y);
		IconMat->SetScalarParameterValue(FName("Index"), InIndex);
	}
}

void UAbilityItemDataBase::ReleaseIconAsset_Implementation()
{
	Icon = nullptr;
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return UAbilityModuleBPLibrary::AssetTypeToItemType(Type);
}
