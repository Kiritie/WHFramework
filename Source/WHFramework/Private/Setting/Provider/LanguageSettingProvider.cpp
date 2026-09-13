#include "Setting/Provider/LanguageSettingProvider.h"

#include "Widget/WidgetModule.h"

void ULanguageSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	const FSettingId LanguageId{FName(TEXT("Widget.LanguageType"))};
	FSettingDefinition* Definition = OutDefinitions.FindByPredicate([LanguageId](const FSettingDefinition& Item)
	{
		return Item.SettingId == LanguageId;
	});
	if(!Definition)
	{
		return;
	}

	Definition->Provider = FName(TEXT("Language"));
	Definition->Renderer = ESettingRendererType::Option;
	Definition->ApplyPolicy = ESettingApplyPolicy::Immediate;
	Definition->Options.Reset();
	const TArray<FLanguageType> LanguageTypes = UWidgetModule::Get().GetLanguageTypes();
	for(int32 Index = 0; Index < LanguageTypes.Num(); ++Index)
	{
		FSettingOption& Option = Definition->Options.AddDefaulted_GetRef();
		Option.Value = FParameter(Index);
		Option.DisplayName = FText::FromString(LanguageTypes[Index].DisplayName);
	}
}
