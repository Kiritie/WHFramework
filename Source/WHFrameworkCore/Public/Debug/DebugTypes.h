#pragma once
#include "DebugTypes.generated.h"

UENUM(BlueprintType)
enum EDebugMode : int
{
	EDM_All,
	EDM_Screen,
	EDM_Console
};

UENUM(BlueprintType)
enum EDebugCategory : int
{
	EDC_Default UMETA(DisplayName = "Default"),
	EDC_Editor UMETA(DisplayName = "Editor"),
	EDC_Controller UMETA(DisplayName = "Controller"),
	EDC_Achievement UMETA(DisplayName = "Achievement"),
	EDC_Ability UMETA(DisplayName = "Ability"),
	EDC_Asset UMETA(DisplayName = "Asset"),
	EDC_Audio UMETA(DisplayName = "Audio"),
	EDC_Camera UMETA(DisplayName = "Camera"),
	EDC_Character UMETA(DisplayName = "Character"),
	EDC_Event UMETA(DisplayName = "Event"),
	EDC_FSM UMETA(DisplayName = "FSM"),
	EDC_Input UMETA(DisplayName = "Input"),
	EDC_Animation UMETA(DisplayName = "Animation"),
	EDC_Media UMETA(DisplayName = "Media"),
	EDC_Network UMETA(DisplayName = "Network"),
	EDC_ObjectPool UMETA(DisplayName = "ObjectPool"),
	EDC_Parameter UMETA(DisplayName = "Parameter"),
	EDC_Platform UMETA(DisplayName = "Platform"),
	EDC_Pawn UMETA(DisplayName = "Pawn"),
	EDC_Procedure UMETA(DisplayName = "Procedure"),
	EDC_ReferencePool UMETA(DisplayName = "ReferencePool"),
	EDC_SaveGame UMETA(DisplayName = "SaveGame"),
	EDC_Scene UMETA(DisplayName = "Scene"),
	EDC_Step UMETA(DisplayName = "Step"),
	EDC_Task UMETA(DisplayName = "Task"),
	EDC_Voxel UMETA(DisplayName = "Voxel"),
	EDC_WebRequest UMETA(DisplayName = "WebRequest"),
	EDC_Widget UMETA(DisplayName = "Widget"),
	EDC_Zip UMETA(DisplayName = "Zip"),
	EDC_Custom1 UMETA(DisplayName = "Custom1"),
	EDC_Custom2 UMETA(DisplayName = "Custom2"),
	EDC_Custom3 UMETA(DisplayName = "Custom3"),
	EDC_Custom4 UMETA(DisplayName = "Custom4"),
	EDC_Custom5 UMETA(DisplayName = "Custom5"),
	EDC_Custom6 UMETA(DisplayName = "Custom6"),
	EDC_Custom7 UMETA(DisplayName = "Custom7"),
	EDC_Custom8 UMETA(DisplayName = "Custom8"),
	EDC_Custom9 UMETA(DisplayName = "Custom9"),
	EDC_Custom10 UMETA(DisplayName = "Custom10")
};

UENUM(BlueprintType)
enum EDebugVerbosity : int
{
	EDV_Log,
	EDV_Warning,
	EDV_Error
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FDebugCategoryState
{
	GENERATED_BODY()

public:
	FORCEINLINE FDebugCategoryState()
	{
		bEnableScreenLog = false;
		bEnableConsoleLog = false;
	}

	FORCEINLINE FDebugCategoryState(bool bInEnableScreenLog, bool bInEnableConsoleLog)
	{
		bEnableScreenLog = bInEnableScreenLog;
		bEnableConsoleLog = bInEnableConsoleLog;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableScreenLog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnableConsoleLog;
};

// 默认
DEFINE_LOG_CATEGORY_STATIC(WH_Default, Log, All);
// 编辑器
DEFINE_LOG_CATEGORY_STATIC(WH_Editor, Log, All);
// 控制器
DEFINE_LOG_CATEGORY_STATIC(WH_Controller, Log, All);
// 能力
DEFINE_LOG_CATEGORY_STATIC(WH_Ability, Log, All);
// 成就
DEFINE_LOG_CATEGORY_STATIC(WH_Achievement, Log, All);
// 资源
DEFINE_LOG_CATEGORY_STATIC(WH_Asset, Log, All);
// 音频
DEFINE_LOG_CATEGORY_STATIC(WH_Audio, Log, All);
// 相机
DEFINE_LOG_CATEGORY_STATIC(WH_Camera, Log, All);
// 角色
DEFINE_LOG_CATEGORY_STATIC(WH_Character, Log, All);
// 事件
DEFINE_LOG_CATEGORY_STATIC(WH_Event, Log, All);
// 状态机
DEFINE_LOG_CATEGORY_STATIC(WH_FSM, Log, All);
// 输入
DEFINE_LOG_CATEGORY_STATIC(WH_Input, Log, All);
// 动画
DEFINE_LOG_CATEGORY_STATIC(WH_Animation, Log, All);
// 媒体
DEFINE_LOG_CATEGORY_STATIC(WH_Media, Log, All);
// 网络
DEFINE_LOG_CATEGORY_STATIC(WH_Network, Log, All);
// 对象池
DEFINE_LOG_CATEGORY_STATIC(WH_ObjectPool, Log, All);
// 参数
DEFINE_LOG_CATEGORY_STATIC(WH_Parameter, Log, All);
// 平台
DEFINE_LOG_CATEGORY_STATIC(WH_Platform, Log, All);
// Pawn
DEFINE_LOG_CATEGORY_STATIC(WH_Pawn, Log, All);
// 流程
DEFINE_LOG_CATEGORY_STATIC(WH_Procedure, Log, All);
// 引用池
DEFINE_LOG_CATEGORY_STATIC(WH_ReferencePool, Log, All);
// 存档
DEFINE_LOG_CATEGORY_STATIC(WH_SaveGame, Log, All);
// 场景
DEFINE_LOG_CATEGORY_STATIC(WH_Scene, Log, All);
// 步骤
DEFINE_LOG_CATEGORY_STATIC(WH_Step, Log, All);
// 任务
DEFINE_LOG_CATEGORY_STATIC(WH_Task, Log, All);
// 体素
DEFINE_LOG_CATEGORY_STATIC(WH_Voxel, Log, All);
// Web请求
DEFINE_LOG_CATEGORY_STATIC(WH_WebRequest, Log, All);
// UI
DEFINE_LOG_CATEGORY_STATIC(WH_Widget, Log, All);
// Zip
DEFINE_LOG_CATEGORY_STATIC(WH_Zip, Log, All);

// 断言实现
#if WITH_EDITOR
	#define WH_ENSUREEDITOR_IMPL(Capture, Always, InExpression, InFormat) \
	(LIKELY(!!(InExpression)) || (DispatchCheckVerify<bool>([&] () UE_DEBUG_SECTION \
	{ \
		static bool bExecuted = false; \
		return CheckVerifyImpl(bExecuted, Always, __FILE__, __LINE__, PLATFORM_RETURN_ADDRESS(), #InExpression, InFormat); \
	}) && [] () { PLATFORM_BREAK(); return false; } ()))

	#define WH_ENSUREEDITORMSGF_IMPL(Capture, Always, InExpression, InFormat, Message, Category, Verbosity) \
	(LIKELY(!!(InExpression)) || (DispatchCheckVerify<bool>([&] () UE_DEBUG_SECTION \
	{ \
		static bool bExecuted = false; \
		if ((!bExecuted)) \
		{ \
			WHLog(Message, Category, Verbosity); \
		} \
		return CheckVerifyImpl(bExecuted, Always, __FILE__, __LINE__, PLATFORM_RETURN_ADDRESS(), #InExpression, InFormat); \
	}) && [] () { PLATFORM_BREAK(); return false; } ()))
#else
	#define WH_ENSUREEDITOR_IMPL(Capture, Always, InExpression, InFormat) \
	(LIKELY(!!(InExpression)) || (DispatchCheckVerify<bool>([&] () UE_DEBUG_SECTION \
	{ \
		static bool bExecuted = false; \
		return false; \
	}) && [] () { PLATFORM_BREAK(); return false; } ()))

	#define WH_ENSUREEDITORMSGF_IMPL(Capture, Always, InExpression, InFormat, Message, Category, Verbosity) \
	(LIKELY(!!(InExpression)) || (DispatchCheckVerify<bool>([&] () UE_DEBUG_SECTION \
	{ \
		static bool bExecuted = false; \
		if ((!bExecuted)) \
		{ \
			WHLog(Message, Category, Verbosity); \
		} \
		return false; \
	}) && [] () { PLATFORM_BREAK(); return false; } ()))
#endif

#define LOG_CASE(Category, Verbosity) \
case EDC_##Category: \
{ \
	switch (Verbosity) \
	{ \
		case EDV_Log: UE_LOG(WH_##Category, Log, TEXT("%s"), *InMessage); break; \
		case EDV_Warning: UE_LOG(WH_##Category, Warning, TEXT("%s"), *InMessage); break; \
		case EDV_Error: UE_LOG(WH_##Category, Error, TEXT("%s"), *InMessage); break; \
	} \
	break; \
}

#define ensureEditor(InExpression) WH_ENSUREEDITOR_IMPL ( , true, InExpression, TEXT(""))

#define ensureEditorMsgf(InExpression, Message, Category, Verbosity) WH_ENSUREEDITORMSGF_IMPL ( , true, InExpression, TEXT(""), Message, Category, Verbosity)

/*
 * 打印日志
 * @param Message 消息内容
 * @param Category 调试类别
 * @param Verbosity 调试级别
 */
WHFRAMEWORKCORE_API void WHLog(const FString& Message, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log);

/*
 * 输出调试信息
 * @param Message 调试内容
 * @param Mode 调试模式
 * @param Category 调试类别
 * @param Verbosity 调试级别
 * @param DisplayColor 显示颜色
 * @param Duration 持续时间
 * @param Key 调试ID
 * @param bNewerOnTop 不更新在顶部
 */
WHFRAMEWORKCORE_API void WHDebug(const FString& Message, EDebugMode Mode = EDM_Screen, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log, const FColor& DisplayColor = FColor::Cyan, float Duration = 1.5f, int32 Key = -1, bool bNewerOnTop = true);
