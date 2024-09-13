#include "Ability/Item/AbilityItemDataBase.h"

#include "Ability/AbilityModule.h"
#include "Common/CommonStatics.h"
#include "Debug/DebugModuleTypes.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = FName("None");

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
	PickUpClass = nullptr;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IconSourceMatFinder(TEXT("/Script/Engine.Material'/WHFramework/Ability/Materials/M_ItemIcon.M_ItemIcon'"));
	if(IconSourceMatFinder.Succeeded())
	{
		IconSourceMat = IconSourceMatFinder.Object;
	}
}

void UAbilityItemDataBase::ResetData_Implementation()
{
	Super::ResetData_Implementation();
}

void UAbilityItemDataBase::SetIconByTexture_Implementation(UTexture* InTexture, FVector2D InSize, int32 InIndex)
{
	if(!IconSourceMat) return;
	
	if(const auto IconMat = UMaterialInstanceDynamic::Create(IconSourceMat, nullptr))
	{
		IconMat->SetTextureParameterValue(FName("Texture"), InTexture);
		IconMat->SetScalarParameterValue(FName("SizeX"), InSize.X);
		IconMat->SetScalarParameterValue(FName("SizeY"), InSize.Y);
		IconMat->SetScalarParameterValue(FName("Index"), InIndex);
		Icon = IconMat;
	}
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return (EAbilityItemType)UCommonStatics::GetEnumValueByAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), Type.ToString());
}
