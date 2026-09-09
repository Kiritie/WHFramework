// Copyright Underflow Studios 2017

#include "Dialogue/Widget/WidgetDialogueBase.h"

#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueEventBase.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "Dialogue/Base/DialogueAsset.h"

bool UWidgetDialogueBase::IsConditionsMetForNode_Implementation(FDialogueNode Node)
{
	for (UDialogueConditionBase* Condition : Node.Conditions)
	{
		if (IsValid(Condition))
		{
			if (!Condition->IsConditionMet(GetOwningPlayer(), NPCActor))
			{
				return false;
			}
		}
	}
	return true;
}

void UWidgetDialogueBase::RunEventsForNode_Implementation(FDialogueNode Node)
{
	for (UDialogueEventBase* Event : Node.Events)
	{
		if (IsValid(Event))
		{
			Event->ReceiveEventTriggered(GetOwningPlayer(), NPCActor);
		}
	}
}

/* If you supply this function with "charname", it'll run the function called Get_charname
 * It'll also make sure that your Get_charname function has no parameters and only returns a string
 * The resulting string will be returned in &resultString
*/
bool UWidgetDialogueBase::RunStringReplacer(FString originalString, FString& resultString)
{
	const FString methodToCall = FString::Printf(TEXT("Get_%s"), *originalString);
	UFunction* Func = GetClass()->FindFunctionByName(FName(*methodToCall), EIncludeSuperFlag::ExcludeSuper);

	if (Func == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Dialogue: Function \"%s\" wasn't found on the dialogue widget."), *methodToCall);
		return false;
	}

	int foundReturnStrings = 0;
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		FProperty* Prop = *It;

		// print property flags
		//uint64 flags = (uint64)Prop->GetPropertyFlags();
		//UE_LOG(LogTemp, Log, TEXT("Property Flags: %llu"), flags); // convert to hex: https://www.rapidtables.com/convert/number/decimal-to-hex.html

		// if it's a return type (in blueprints it's an out parameter), check that it's a string
		if (Prop->HasAllPropertyFlags(CPF_Parm | CPF_OutParm))
		{
			if (!Prop->GetCPPType().Equals(TEXT("FString")))
			{
				// if we land here, it means our method returns something other than a string
				UE_LOG(LogTemp, Error, TEXT("Dialogue: Your method \"%s\" is returning something other than a string!"), *methodToCall);
				return false;
			}
			else
			{
				foundReturnStrings++;
			}
		}
		// if it's a normal parameter, return false
		else if (Prop->HasAnyPropertyFlags(CPF_Parm) && !Prop->HasAnyPropertyFlags(CPF_OutParm))
		{
			// we have some parameters, but we shouldn't have them
			UE_LOG(LogTemp, Error, TEXT("Dialogue: Your method \"%s\" must have no parameters!"), *methodToCall);
			return false;
		}
	}
	if (foundReturnStrings > 1)
	{
		UE_LOG(LogTemp, Error, TEXT("Dialogue: Your method \"%s\" must return only one string!"), *methodToCall);
		return false;
	}
	else if (foundReturnStrings == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Dialogue: Your method \"%s\" doesn't return anything, but must return a string!"), *methodToCall);
		return false;
	}

	FString retvalue;
	ProcessEvent(Func, &retvalue);
	resultString = retvalue;

	return true;
}

/* In the supplied inText, finds all strings of that fit the pattern %string% (a single word between two percentage signs)
 * and returns an array of them without the percentage sign.
*/
TArray<FString> UWidgetDialogueBase::FindVarStrings(FText inText)
{
	TArray<FString> varStrings;

	FString totalText = inText.ToString();

	int firstPercent = -1;
	for (int i = 0; i < totalText.Len(); i++)
	{
		if (totalText[i] == '%')
		{
			if (firstPercent == -1) // if we encounter the first % sign
			{
				firstPercent = i;
			}
			else if (firstPercent + 1 == i) //if we encounter "%%", disregard the first one
			{
				firstPercent = i;
			}
			else // if we encounter second % sign
			{
				FString foundVarString = totalText.Mid(firstPercent + 1, i - firstPercent - 1);
				varStrings.AddUnique(foundVarString);
				firstPercent = -1;
			}
		}
		if (totalText[i] == ' ')
		{
			firstPercent = -1;
		}
	}

	return varStrings;
}
