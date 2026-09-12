#include "Input/Widget/WidgetInputAction.h"

#include "Input/Base/InputActionBase.h"
#include "Input/InputModule.h"

UWidgetInputAction::UWidgetInputAction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWidgetInputAction::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetActionTag(ActionTag);
}

void UWidgetInputAction::SetActionTag(FGameplayTag InActionTag)
{
	ActionTag = InActionTag;
	const UInputActionBase* InputAction = ActionTag.IsValid()
		? UInputModule::Get().GetInputActionByTag(ActionTag)
		: nullptr;
	SetEnhancedInputAction(const_cast<UInputActionBase*>(InputAction));
}
