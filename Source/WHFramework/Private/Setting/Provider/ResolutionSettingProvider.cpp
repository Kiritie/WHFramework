#include "Setting/Provider/ResolutionSettingProvider.h"

#include "DynamicRHI.h"
#include "GenericPlatform/GenericApplication.h"

void UResolutionSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	const FSettingId ResolutionId{FName(TEXT("Video.WindowResolution"))};
	FSettingDefinition* Definition = OutDefinitions.FindByPredicate([ResolutionId](const FSettingDefinition& Item)
	{
		return Item.SettingId == ResolutionId;
	});
	if(!Definition)
	{
		return;
	}

	TArray<FIntPoint> Resolutions;
	if(GDynamicRHI)
	{
		FScreenResolutionArray AvailableResolutions;
		if(RHIGetAvailableResolutions(AvailableResolutions, true))
		{
			for(const FScreenResolutionRHI& Resolution : AvailableResolutions)
			{
				Resolutions.AddUnique(FIntPoint(Resolution.Width, Resolution.Height));
			}
		}
	}

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	if(DisplayMetrics.PrimaryDisplayWidth > 0 && DisplayMetrics.PrimaryDisplayHeight > 0)
	{
		Resolutions.Remove(FIntPoint(DisplayMetrics.PrimaryDisplayWidth, DisplayMetrics.PrimaryDisplayHeight));
	}
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X == B.X ? A.Y < B.Y : A.X < B.X;
	});

	Definition->Provider = FName(TEXT("Resolution"));
	Definition->Renderer = ESettingRendererType::Option;
	Definition->ApplyPolicy = ESettingApplyPolicy::Deferred;
	Definition->bRequiresConfirmation = true;
	Definition->Options.Reset();
	if(DisplayMetrics.PrimaryDisplayWidth > 0 && DisplayMetrics.PrimaryDisplayHeight > 0)
	{
		FSettingOption& DesktopOption = Definition->Options.AddDefaulted_GetRef();
		DesktopOption.Value = FParameter(FIntPoint::ZeroValue);
		DesktopOption.DisplayName = FText::Format(
			NSLOCTEXT("WH.ResolutionSettingProvider", "DesktopResolution", "桌面分辨率 ({0}x{1})"),
			DisplayMetrics.PrimaryDisplayWidth,
			DisplayMetrics.PrimaryDisplayHeight);
	}
	for(const FIntPoint& Resolution : Resolutions)
	{
		FSettingOption& Option = Definition->Options.AddDefaulted_GetRef();
		Option.Value = FParameter(Resolution);
		Option.DisplayName = FText::FromString(FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y));
	}
}
