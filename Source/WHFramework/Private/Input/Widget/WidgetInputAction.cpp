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
	const UInputModule* Input = UInputModule::GetPtr(GetWorld());
	const UInputActionBase* InputAction = Input && ActionTag.IsValid()
		? Input->GetInputActionByTag(ActionTag, false)
		: nullptr;
	SetEnhancedInputAction(const_cast<UInputActionBase*>(InputAction));
}
