#pragma once

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextScroller.h"

// 补足 CommonUI 私有滚动样式的赋值入口，以稳定外层容器承载原生滚动组件重建。
class FCommonTextScrollStyleBinding
{
public:
	TSharedRef<SWidget> Build(TFunctionRef<TSharedRef<SWidget>()> Rebuild)
	{
		SAssignNew(Host, SBox)[Rebuild()];
		return Host.ToSharedRef();
	}

	void Release()
	{
		Host.Reset();
	}

	void Apply(UWidget* Widget, UClass* StyleClass, TFunctionRef<TSharedRef<SWidget>()> Rebuild)
	{
		FClassProperty* Property = FindFProperty<FClassProperty>(Widget->GetClass(), TEXT("ScrollStyle"));
		if (!ensureMsgf(Property && Property->MetaClass == UCommonTextScrollStyle::StaticClass(), TEXT("CommonUI scroll-style property has changed")))
		{
			return;
		}
		const UCommonTextScrollStyle* Defaults = StyleClass ? StyleClass->GetDefaultObject<UCommonTextScrollStyle>() : nullptr;
		const FTextScrollerOptions Options = Defaults ? Defaults->ToScrollOptions() : FTextScrollerOptions();
		const EWidgetClipping Clipping = Defaults ? Defaults->Clipping : EWidgetClipping::Inherit;
		const bool bChanged = Property->GetObjectPropertyValue_InContainer(Widget) != StyleClass || !bApplied ||
			Options.Speed != LastOptions.Speed || Options.StartDelay != LastOptions.StartDelay || Options.EndDelay != LastOptions.EndDelay ||
			Options.FadeInDelay != LastOptions.FadeInDelay || Options.FadeOutDelay != LastOptions.FadeOutDelay || Clipping != LastClipping;
		if (!bChanged)
		{
			return;
		}
		bApplied = true;
		LastOptions = Options;
		LastClipping = Clipping;
		if (Host.IsValid())
		{
			FBoolProperty* Enabled = FindFProperty<FBoolProperty>(Widget->GetClass(), TEXT("bIsScrollingEnabled"));
			if (!ensure(Enabled))
			{
				return;
			}
			const bool bWasEnabled = Enabled->GetPropertyValue_InContainer(Widget);
			if (UCommonTextBlock* Text = Cast<UCommonTextBlock>(Widget))
			{
				Text->SetScrollingEnabled(false);
			}
			else if (UCommonRichTextBlock* RichText = Cast<UCommonRichTextBlock>(Widget))
			{
				RichText->SetScrollingEnabled(false);
			}
			Enabled->SetPropertyValue_InContainer(Widget, bWasEnabled);
		}
		Property->SetObjectPropertyValue_InContainer(Widget, StyleClass);
		if (Host.IsValid())
		{
			Host->SetContent(Rebuild());
		}
	}

private:
	TSharedPtr<SBox> Host;
	FTextScrollerOptions LastOptions;
	EWidgetClipping LastClipping = EWidgetClipping::Inherit;
	bool bApplied = false;
};
