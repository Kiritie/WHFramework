#pragma once

#include "Widget/ScreenWidgetTypes.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/WidgetModule.h"
#include "Debug/DebugModuleTypes.h"

namespace WidgetParameterHelpers
{
	inline FParameter MakeWidgetSpawnParam(const FParameter& InParam)
	{
		if(!InParam.HasValue())
		{
			return FParameter(FWidgetSpawnParameter());
		}

		if(!InParam.GetPtr<FWidgetSpawnParameter>())
		{
			ensureEditorMsgf(
			false,
			FString::Printf(
				TEXT("Widget parameter %s must derive from FWidgetSpawnParameter."),
				*GetNameSafe(InParam.GetValueStruct())),
			EDC_Widget,
			EDV_Error);
			return FParameter(FWidgetSpawnParameter());
		}
		return InParam;
	}

	inline FParameter MakeWidgetOpenParam(const FParameter& InParam)
	{
		if(!InParam.HasValue())
		{
			return FParameter(FWidgetOpenParameter());
		}

		if(!InParam.GetPtr<FWidgetOpenParameter>())
		{
			ensureEditorMsgf(
				false,
				FString::Printf(
					TEXT("Widget parameter %s must derive from FWidgetOpenParameter."),
					*GetNameSafe(InParam.GetValueStruct())),
				EDC_Widget,
				EDV_Error);
			return FParameter(FWidgetOpenParameter());
		}
		return InParam;
	}

	inline UObject* GetWidgetOwnerObject(const FParameter& InParam)
	{
		if(const FWidgetSpawnParameter* Param = InParam.GetPtr<FWidgetSpawnParameter>())
		{
			return Param->OwnerObject.Get();
		}
		if(const FWidgetOpenParameter* Param = InParam.GetPtr<FWidgetOpenParameter>())
		{
			return Param->OwnerObject.Get();
		}
		return nullptr;
	}

	inline FParameter MakeWidgetSpawnOwnerParam(const FParameter& InParam)
	{
		return FParameter(FWidgetSpawnParameter(GetWidgetOwnerObject(InParam)));
	}

}

