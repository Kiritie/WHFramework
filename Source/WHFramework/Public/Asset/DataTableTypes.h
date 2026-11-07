#pragma once

#include "Engine/DataTable.h"
#include "DataTableTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDataTableRowBase : public FTableRowBase
{
	GENERATED_BODY()

public:
	FDataTableRowBase()
	{
		ID = NAME_None;
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FName ID;

public:
	virtual bool IsValid() const
	{
		return !ID.IsNone();
	}

	virtual void OnInitializeRow(const FName& InRowName);
};
