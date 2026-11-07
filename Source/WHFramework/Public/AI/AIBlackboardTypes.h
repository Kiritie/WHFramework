#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlackboardValuePreChange, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlackboardValueChanged, FName);
