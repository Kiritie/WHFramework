#include "Ability/Item/AbilityItemDataBase.h"

#include "Ability/AbilityModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Abilities/ItemAbilityBase.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "Common/CommonStatics.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = FName("Misc");

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Rarity = EAbilityItemRarity::None;
	Icon = nullptr;
	MaxCount = 0;
	MaxLevel = 0;
	AbilityClass = nullptr;
	PickUpClass = nullptr;

	_Icon = nullptr;
}

void UAbilityItemDataBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	_Icon = Icon;
}

void UAbilityItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();

	Icon = _Icon;
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

FString UAbilityItemDataBase::GetItemAbilityInfo(int32 InLevel) const
{
	if(AbilityClass)
	{
		FString AbilityInfoStr;
		FAbilityInfo AbilityInfo = UAbilityModuleStatics::GetAbilityInfoByClass(AbilityClass, InLevel);
		for(auto& Iter1 : AbilityInfo.Effects)
		{
			for(auto& Iter2 : Iter1.Attributes)
			{
				FText AttributeName = UCommonStatics::GetPropertyDisplayName(Iter2.Attribute.GetUProperty());
				if(Iter2.BaseAttribute.IsValid())
				{
					FText BaseAttributeName = UCommonStatics::GetPropertyDisplayName(Iter2.BaseAttribute.GetUProperty());
					AbilityInfoStr.Appendf(TEXT("%s: %s%s%s\n"), *AttributeName.ToString(), *FString::Printf(TEXT("%d%%"), (int32)(Iter2.Value * 100)), Iter2.AttributeSource == EGameplayEffectAttributeCaptureSource::Source ? TEXT("自身") : TEXT("目标"), *BaseAttributeName.ToString());
				}
				else
				{
					AbilityInfoStr.Appendf(TEXT("%s: %s%s\n"), *AttributeName.ToString(), (Iter2.Attribute.GetName().EndsWith(TEXT("Rate")) ? *FString::Printf(TEXT("%s%%"), *UCommonStatics::SanitizeFloat(Iter2.Value * 100, 2)) : *UCommonStatics::SanitizeFloat(Iter2.Value, 2)),
						(Iter2.Attribute.GetName().EndsWith(TEXT("Time")) || Iter2.Attribute.GetName().EndsWith(TEXT("Interrupt")) ? TEXT("s") : TEXT("")));
				}
			}
			if(Iter1.Period > 0.f)
			{
				AbilityInfoStr.Appendf(TEXT("%触发周期: %ss\n"), *UCommonStatics::SanitizeFloat(Iter1.Period, 2));
			}
			if(Iter1.Duration > 0.f)
			{
				AbilityInfoStr.Appendf(TEXT("%持续时间: %ss\n"), *UCommonStatics::SanitizeFloat(Iter1.Duration, 2));
			}
		}
		if(AbilityInfo.CooldownDuration > 0.f)
		{
			AbilityInfoStr.Appendf(TEXT("冷却时间: %ss\n"), *UCommonStatics::SanitizeFloat(AbilityInfo.CooldownDuration, 2));
		}
		if(FMath::Abs(AbilityInfo.CostValue) > 0.f)
		{
			FText AttributeName = UCommonStatics::GetPropertyDisplayName(AbilityInfo.CostAttribute.GetUProperty());
			AbilityInfoStr.Appendf(TEXT("消耗: %s%s\n"), *UCommonStatics::SanitizeFloat(FMath::Abs(AbilityInfo.CostValue), 2), *AttributeName.ToString());
		}
		
		AbilityInfoStr.RemoveFromEnd(TEXT("\n"));
		
		return AbilityInfoStr;
	}
	return TEXT("");
}

FString UAbilityItemDataBase::GetItemErrorInfo(AActor* InOwner, int32 InLevel) const
{
	FString ErrorInfo;
	if(IAbilityActorInterface* AbilityActor = Cast<IAbilityActorInterface>(InOwner))
	{
		if(AbilityActor->GetLevelA() < InLevel)
		{
			ErrorInfo = FString::Printf(TEXT("角色未达到[%d]级"), InLevel);
		}
	}
	return ErrorInfo;
}
