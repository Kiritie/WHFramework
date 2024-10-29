#include "Ability/Item/AbilityItemDataBase.h"

#include "Ability/AbilityModule.h"
#include "Common/CommonStatics.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = FName("None");

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Icon = nullptr;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
	PickUpClass = nullptr;

	InitIcon = nullptr;
	
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IconSourceMatFinder(TEXT("/Script/Engine.Material'/WHFramework/Ability/Materials/M_ItemIcon.M_ItemIcon'"));
	if(IconSourceMatFinder.Succeeded())
	{
		IconMat = IconSourceMatFinder.Object;
	}
}

void UAbilityItemDataBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	InitIcon = Icon;
}

void UAbilityItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	Icon = InitIcon;
}

int32 UAbilityItemDataBase::ClampCount(int32 InCount) const
{
	return FMath::Clamp(InCount, 0, MaxCount != -1 ? MaxCount : InCount);
}

int32 UAbilityItemDataBase::ClampLevel(int32 InLevel) const
{
	return FMath::Clamp(InLevel, 0, MaxLevel != -1 ? MaxLevel : InLevel);
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return (EAbilityItemType)UCommonStatics::GetEnumValueByAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), Type.ToString());
}

void UAbilityItemDataBase::SetIconByTexture_Implementation(UTexture* InTexture, FVector2D InSize, int32 InIndex)
{
	if(!IconMat) return;

	if(const auto Mat = UMaterialInstanceDynamic::Create(IconMat, nullptr))
	{
		Mat->SetTextureParameterValue(FName("Texture"), InTexture);
		Mat->SetScalarParameterValue(FName("SizeX"), InSize.X);
		Mat->SetScalarParameterValue(FName("SizeY"), InSize.Y);
		Mat->SetScalarParameterValue(FName("Index"), InIndex);
		Icon = Mat;
	}
}
