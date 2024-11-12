#include "Ability/Item/AbilityItemDataBase.h"

#include "Ability/AbilityModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
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

FText UAbilityItemDataBase::GetItemAttributeInfo(int32 InLevel) const
{
	if(AbilityClass)
	{
		FString AttributeInfo;
		FAbilityInfo AbilityInfo = UAbilityModuleStatics::GetAbilityInfoByClass(AbilityClass, InLevel);
		for(auto& Iter1 : AbilityInfo.Effects)
		{
			for(auto& Iter2 : Iter1.Attributes)
			{
				FText AttributeName = UCommonStatics::GetPropertyDisplayName(Iter2.Attribute.GetUProperty());
				if(Iter2.BaseAttribute.IsValid())
				{
					FText BaseAttributeName = UCommonStatics::GetPropertyDisplayName(Iter2.BaseAttribute.GetUProperty());
					AttributeInfo.Appendf(TEXT("%s: %s%s%s\n"), *AttributeName.ToString(), *FString::Printf(TEXT("%d%%"), (int32)(Iter2.Value * 100)), Iter2.AttributeSource == EGameplayEffectAttributeCaptureSource::Source ? TEXT("自身") : TEXT("目标"), *BaseAttributeName.ToString());
				}
				else
				{
					AttributeInfo.Appendf(TEXT("%s: %s\n"), *AttributeName.ToString(), !Iter2.Attribute.GetName().EndsWith(TEXT("Rate")) ? *FString::SanitizeFloat(Iter2.Value, 0) : *FString::Printf(TEXT("%s%%"), *FString::SanitizeFloat(Iter2.Value * 100, 0)));
				}
			}
			if(Iter1.Period > 0.f)
			{
				AttributeInfo.Appendf(TEXT("%触发周期: %s\n"), *FString::SanitizeFloat(Iter1.Period, 0));
			}
			if(Iter1.Duration > 0.f)
			{
				AttributeInfo.Appendf(TEXT("%持续时间: %s\n"), *FString::SanitizeFloat(Iter1.Duration, 0));
			}
		}
		if(AbilityInfo.CooldownDuration > 0.f)
		{
			AttributeInfo.Appendf(TEXT("冷却时间: %s\n"), *FString::SanitizeFloat(AbilityInfo.CooldownDuration, 0));
		}
		if(FMath::Abs(AbilityInfo.CostValue) > 0.f)
		{
			FText AttributeName = UCommonStatics::GetPropertyDisplayName(AbilityInfo.CostAttribute.GetUProperty());
			AttributeInfo.Appendf(TEXT("消耗: %s%s\n"), *FString::SanitizeFloat(FMath::Abs(AbilityInfo.CostValue), 0), *AttributeName.ToString());
		}
		
		AttributeInfo.RemoveFromEnd(TEXT("\n"));
		
		return FText::FromString(AttributeInfo);
	}
	return FText::GetEmpty();
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
