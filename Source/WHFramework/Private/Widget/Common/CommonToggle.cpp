#include "Widget/Common/CommonToggle.h"

#include "Blueprint/WidgetTree.h"
#include "Widget/Style/CommonToggleStyle.h"

UCommonToggle::UCommonToggle(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

EWidgetStyleKind UCommonToggle::GetStyleKind() const
{
	return EWidgetStyleKind::Toggle;
}

void UCommonToggle::BuildNativeLayout()
{
	Chk_Value = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("Chk_Value"));
	WidgetTree->RootWidget = Chk_Value;
}

void UCommonToggle::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetCheckedState(CheckedState);
}

void UCommonToggle::ApplyResolvedStyle(const UObject* InStyle)
{
	const UCommonToggleStyle* Style = Cast<UCommonToggleStyle>(InStyle);
	if (Style && Chk_Value)
	{
		Chk_Value->SetWidgetStyle(Style->CheckBoxStyle);
	}
}

void UCommonToggle::BindControlEvents()
{
	if (Chk_Value)
	{
		Chk_Value->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCheckChanged);
	}
}

void UCommonToggle::UnbindControlEvents()
{
	if (Chk_Value)
	{
		Chk_Value->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleCheckChanged);
	}
}

void UCommonToggle::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	Super::OnDespawn_Implementation(InMode);
	OnValueChanged.Clear();
	SetCheckedState(CastChecked<UCommonToggle>(GetArchetype())->CheckedState);
}

void UCommonToggle::HandleCheckChanged(bool bInChecked)
{
	CheckedState = Chk_Value ? Chk_Value->GetCheckedState() : ECheckBoxState::Unchecked;
	if (!bSynchronizingValue && CheckedState != ECheckBoxState::Undetermined)
	{
		OnValueChanged.Broadcast(this, bInChecked);
	}
}

void UCommonToggle::SetCheckedState(ECheckBoxState InState, bool bNotify)
{
	TGuardValue<bool> Guard(bSynchronizingValue, true);
	CheckedState = InState;
	if (Chk_Value)
	{
		Chk_Value->SetCheckedState(InState);
	}
	if (bNotify && InState != ECheckBoxState::Undetermined)
	{
		OnValueChanged.Broadcast(this, InState == ECheckBoxState::Checked);
	}
}

ECheckBoxState UCommonToggle::GetCheckedState() const
{
	return Chk_Value ? Chk_Value->GetCheckedState() : CheckedState;
}

void UCommonToggle::SetIsChecked(bool bInChecked, bool bNotify)
{
	SetCheckedState(bInChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked, bNotify);
}

bool UCommonToggle::IsChecked() const
{
	return GetCheckedState() == ECheckBoxState::Checked;
}
