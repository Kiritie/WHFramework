#pragma once

#include "CoreMinimal.h"
#include "DebugModuleTypes.generated.h"

UENUM(BlueprintType)
enum class EStatusLevel : uint8
{
	//普通
	ERF_Log		UMETA(DisplayName = "ERF_Log"),
	//警告
	ERF_Warning		UMETA(DisplayName = "ERF_Warning"),
	//错误
	ERF_Error			UMETA(DisplayName = "ERF_Error"),
};
int32 EStatusLevelToInt(EStatusLevel StatusLevel);

/**
 * DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity);
 * CategoryName:分类名称
 * DefaultVerbosity:默认显示等级
 * CompileTimeVerbosity:最大显示等级
 */

// 流程
DEFINE_LOG_CATEGORY_STATIC(WHProcedureFlow, Log, All);

// 控制器
DEFINE_LOG_CATEGORY_STATIC(WHPlayerController, Log, All);

// 事件
DEFINE_LOG_CATEGORY_STATIC(WHEvent, Log, All);

// HTTP请求
DEFINE_LOG_CATEGORY_STATIC(WHHTTP, Log, All);

#define WH_LOG(CategoryName, Verbosity, Format, ...) { UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); }
#define WHStatusLog(CategoryName, device_status_code, device_status_level, Format, ...) \
	{ \
		if (device_status_level == EStatusLevel::ERF_Warning) { UE_LOG(CategoryName, Warning, Format, ##__VA_ARGS__); }\
		else if (device_status_level == EStatusLevel::ERF_Error) { UE_LOG(CategoryName, Error, Format, ##__VA_ARGS__); }\
		else { UE_LOG(CategoryName, Log, Format, ##__VA_ARGS__); }\
		FString __Z__LogContent = FString::Printf(Format, ##__VA_ARGS__); \
		FWHLog::StatusLog(device_status_code, device_status_level, __Z__LogContent);\
	}

class IVREALKIT_API FWHLog 
{
public:

	static void StatusLog(int32 device_status_code, EStatusLevel device_status_level, const FString& device_status_info);
};

// 用这两个断言, 代替多数, 仅需要在编辑器中进行判断的断言
#if WITH_EDITOR
	#define ensureEditor(InExpression) ensureAlways(InExpression)
	#define ensureEditorMsgf( InExpression, InFormat, ... ) ensureAlwaysMsgf( InExpression, InFormat, ##__VA_ARGS__)
#else
	#define ensureEditor(InExpression) IVREAL_ENSURE_IMPL( , InExpression, TEXT(""))
	#define ensureEditorMsgf( InExpression, InFormat, ... ) IVREAL_ENSURE_IMPL(&, InExpression, InFormat, ##__VA_ARGS__)
#endif