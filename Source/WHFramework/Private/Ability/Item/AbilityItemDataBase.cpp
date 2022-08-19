#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::None);

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Icon = nullptr;
	IconMat = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IconSourceMatFinder(TEXT("Material'/WHFramework/Ability/Item/Materials/M_ItemIcon.M_ItemIcon'"));
	if(IconSourceMatFinder.Succeeded())
	{
		IconSourceMat = IconSourceMatFinder.Object;
	}
}

void UAbilityItemDataBase::InitIconMat(UTexture* InTex, int32 InTexSize, int32 InIndex)
{
	if(!IconSourceMat) return;
	
	IconMat = UMaterialInstanceDynamic::Create(IconSourceMat, this);
	IconMat->SetTextureParameterValue(FName("Texture"), InTex);
	IconMat->SetScalarParameterValue(FName("TexSize"), InTexSize);
	IconMat->SetScalarParameterValue(FName("Index"), InIndex);
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return UAbilityModuleBPLibrary::AssetTypeToItemType(Type);
}
