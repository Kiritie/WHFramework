#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Widget/Common/WidgetBlueprintEditorLibrary.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "UObject/StrongObjectPtr.h"
#include "Widget/Common/CommonButton.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetBlueprintTemplateIdentifiersTest, "WHFramework.WidgetBlueprint.TemplateIdentifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetBlueprintTemplateIdentifiersTest::RunTest(const FString& Parameters)
{
	const FName Name(*FString::Printf(TEXT("WidgetTemplateTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UWidgetBlueprint> Blueprint(FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
		GetTransientPackage(), Name, BPTYPE_Normal, UUserWidget::StaticClass(), nullptr, NAME_None, false));
	if (!TestNotNull(TEXT("Transient source blueprint is created"), Blueprint.Get()))
	{
		return false;
	}
	FString Error;
	UVerticalBox* Root = Cast<UVerticalBox>(UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), UVerticalBox::StaticClass(), TEXT("Root"), Error));
	UWidget* Label = UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), UTextBlock::StaticClass(), TEXT("Label"), Error);
	if (!TestNotNull(TEXT("Root template is created"), Root) || !TestNotNull(TEXT("Label template is created"), Label))
	{
		return false;
	}
	Root->AddChild(Label);
	TestTrue(TEXT("Root is assigned"), UWidgetBlueprintEditorLibrary::SetRootWidget(Blueprint.Get(), Root, Error));
	TestTrue(TEXT("Identifiers exist before compilation"), UWidgetBlueprintEditorLibrary::ValidateWidgetIdentifiers(Blueprint.Get(), Error));
	const FGuid OriginalId = Blueprint->WidgetVariableNameToGuidMap.FindChecked(TEXT("Label"));
	TestTrue(TEXT("Source tree compiles without compiler identity repair"), UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Blueprint.Get(), Error));
	TestEqual(TEXT("Compilation preserves the source identifier"), Blueprint->WidgetVariableNameToGuidMap.FindChecked(TEXT("Label")), OriginalId);
	Blueprint->WidgetVariableNameToGuidMap.Remove(TEXT("Label"));
	TestFalse(TEXT("Missing identity is rejected before entering the compiler"), UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Blueprint.Get(), Error));
	TestTrue(TEXT("The failed preflight identifies the widget"), Error.Contains(TEXT("Label")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetBlueprintRelatedReplacementTest, "WHFramework.WidgetBlueprint.RelatedTemplateReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWidgetBlueprintRelatedReplacementTest::RunTest(const FString& Parameters)
{
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TStrongObjectPtr<UWidgetBlueprint> Wrapper(FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
		GetTransientPackage(), FName(*(TEXT("ButtonWrapper_") + Suffix)), BPTYPE_Normal, UCommonButton::StaticClass(), nullptr, NAME_None, false));
	TStrongObjectPtr<UWidgetBlueprint> Blueprint(FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
		GetTransientPackage(), FName(*(TEXT("WidgetReplacement_") + Suffix)), BPTYPE_Normal, UUserWidget::StaticClass(), nullptr, NAME_None, false));
	FString Error;
	if (!Wrapper || !Blueprint || !UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Wrapper.Get(), Error))
	{
		AddError(TEXT("Could not prepare transient related widget classes: ") + Error);
		return false;
	}
	UVerticalBox* Root = Cast<UVerticalBox>(UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), UVerticalBox::StaticClass(), TEXT("Root"), Error));
	UWidget* First = UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), Wrapper->GeneratedClass.Get(), TEXT("First"), Error);
	UWidget* Second = UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), Wrapper->GeneratedClass.Get(), TEXT("Second"), Error);
	UWidget* Decoration = UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), UVerticalBox::StaticClass(), TEXT("Decoration"), Error);
	if (!Root || !First || !Second || !Decoration)
	{
		AddError(TEXT("Could not prepare source widgets: ") + Error);
		return false;
	}
	First->bIsVariable = true;
	Second->bIsVariable = true;
	Root->AddChildToVerticalBox(First)->SetPadding(FMargin(9.f));
	Root->AddChild(Second);
	Root->AddChild(Decoration);
	UWidgetBlueprintEditorLibrary::SetRootWidget(Blueprint.Get(), Root, Error);
	if (!UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Blueprint.Get(), Error))
	{
		AddError(Error);
		return false;
	}
	UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint.Get(), TEXT("ReplacementEvents"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint.Get(), Graph);
	const FMulticastDelegateProperty* Delegate = FindFProperty<FMulticastDelegateProperty>(UCommonButton::StaticClass(), TEXT("OnButtonBaseClicked"));
	for (FName ComponentName : {FName(TEXT("First")), FName(TEXT("Second"))})
	{
		const FObjectProperty* Component = FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, ComponentName);
		if (!Component || !Delegate)
		{
			AddError(TEXT("The source widget event contract is missing."));
			return false;
		}
		UK2Node_ComponentBoundEvent* Event = NewObject<UK2Node_ComponentBoundEvent>(Graph);
		Graph->AddNode(Event, false, false);
		Event->CreateNewGuid();
		Event->InitializeComponentBoundEventParams(Component, Delegate);
		Event->AllocateDefaultPins();
	}
	UK2Node_ComponentBoundEvent* FirstEvent = CastChecked<UK2Node_ComponentBoundEvent>(Graph->Nodes[0]);
	UK2Node_VariableGet* Variable = NewObject<UK2Node_VariableGet>(Graph);
	Graph->AddNode(Variable, false, false);
	Variable->CreateNewGuid();
	Variable->VariableReference.SetSelfMember(TEXT("First"));
	Variable->AllocateDefaultPins();
	UK2Node_CallFunction* Action = NewObject<UK2Node_CallFunction>(Graph);
	Graph->AddNode(Action, false, false);
	Action->CreateNewGuid();
	Action->SetFromFunction(UWidget::StaticClass()->FindFunctionByName(TEXT("SetIsEnabled")));
	Action->AllocateDefaultPins();
	UEdGraphPin* EventOutput = FirstEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	EventOutput->MakeLinkTo(Action->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
	Variable->GetValuePin()->MakeLinkTo(Action->FindPinChecked(UEdGraphSchema_K2::PN_Self));
	if (!UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Blueprint.Get(), Error))
	{
		AddError(Error);
		return false;
	}
	const FGuid WidgetId = Blueprint->WidgetVariableNameToGuidMap.FindChecked(TEXT("First"));
	const FGuid EventId = FirstEvent->NodeGuid;
	const FGuid OutputId = FirstEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then)->PinId;
	if (!TestTrue(TEXT("First derived template is replaced"), UWidgetBlueprintEditorLibrary::ReplaceWidget(Blueprint.Get(), First, UCommonButton::StaticClass(), Error)))
	{
		AddError(Error);
		return false;
	}
	First = UWidgetBlueprintEditorLibrary::FindWidget(Blueprint.Get(), TEXT("First"));
	TestEqual(TEXT("Replacement retains parent slot padding"), CastChecked<UVerticalBoxSlot>(First->Slot)->GetPadding(), FMargin(9.f));
	First->RemoveFromParent();
	TestTrue(TEXT("Second replacement does not compile the temporarily detached first widget"), UWidgetBlueprintEditorLibrary::ReplaceWidget(Blueprint.Get(), Second, UCommonButton::StaticClass(), Error));
	TestTrue(TEXT("Decoration removal does not compile the incomplete tree"), UWidgetBlueprintEditorLibrary::RemoveWidget(Blueprint.Get(), Decoration, Error));
	UVerticalBox* NewContainer = Cast<UVerticalBox>(UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), UVerticalBox::StaticClass(), TEXT("NewContainer"), Error));
	TestTrue(TEXT("New layout container attaches without compiling the detached bound widget"), UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint.Get(), NewContainer, Root, -1, Error));
	TestTrue(TEXT("Detached bound widget moves without compiling the incomplete tree"), UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint.Get(), First, NewContainer, -1, Error));
	UVerticalBox* InnerContainer = Cast<UVerticalBox>(UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint.Get(), UVerticalBox::StaticClass(), TEXT("InnerContainer"), Error));
	TestTrue(TEXT("Nested layout container attaches"), UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint.Get(), InnerContainer, NewContainer, -1, Error));
	TestFalse(TEXT("A layout container cannot move into its descendant"), UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint.Get(), NewContainer, InnerContainer, -1, Error));
	TestFalse(TEXT("The root cannot move below another source widget"), UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint.Get(), Root, NewContainer, -1, Error));
	TestFalse(TEXT("An out of range layout position is rejected"), UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint.Get(), Second, NewContainer, 100, Error));
	TestFalse(TEXT("Structural suppression is restored after each edit"), Blueprint->bSuppressStructurallyModified);
	TestTrue(TEXT("Source GUID is retained before compilation"), Blueprint->WidgetVariableNameToGuidMap.FindChecked(TEXT("First")) == WidgetId);
	TestTrue(TEXT("The complete replacement tree compiles"), UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Blueprint.Get(), Error));
	TestEqual(TEXT("Both component events and action nodes remain"), Graph->Nodes.Num(), 4);
	TestEqual(TEXT("Event node identity is retained"), FirstEvent->NodeGuid, EventId);
	EventOutput = FirstEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	TestEqual(TEXT("Event pin identity is retained"), EventOutput->PinId, OutputId);
	TestTrue(TEXT("Event execution connection is retained"), EventOutput->LinkedTo.Contains(Action->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
	TestTrue(TEXT("Action target connection is retained"), Variable->GetValuePin()->LinkedTo.Contains(Action->FindPinChecked(UEdGraphSchema_K2::PN_Self)));
	return true;
}
#endif
