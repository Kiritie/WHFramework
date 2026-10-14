#pragma once

#include "CoreMinimal.h"
#include "WidgetStyleTypes.generated.h"

UENUM(BlueprintType)
enum class EWidgetStyleKind : uint8
{
	None,
	Button,
	Text,
	Brush
};
