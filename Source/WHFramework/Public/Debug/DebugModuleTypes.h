#pragma once

#include "DebugModuleStatics.h"

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
// 潜行任务
DEFINE_LOG_CATEGORY_STATIC(WH_LatentAction, Log, All);
// 媒体
DEFINE_LOG_CATEGORY_STATIC(WH_Media, Log, All);
// 网络
DEFINE_LOG_CATEGORY_STATIC(WH_Network, Log, All);
// 对象池
DEFINE_LOG_CATEGORY_STATIC(WH_ObjectPool, Log, All);
// 参数
DEFINE_LOG_CATEGORY_STATIC(WH_Parameter, Log, All);
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

// 断言实现
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
WHLog(Message, Category, Verbosity); \
return CheckVerifyImpl(bExecuted, Always, __FILE__, __LINE__, PLATFORM_RETURN_ADDRESS(), #InExpression, InFormat); \
}) && [] () { PLATFORM_BREAK(); return false; } ()))

#define ensureEditor(InExpression) WH_ENSUREEDITOR_IMPL ( , true, InExpression, TEXT(""))

#define ensureEditorMsgf(InExpression, Message, Category, Verbosity) WH_ENSUREEDITORMSGF_IMPL ( , true, InExpression, TEXT(""), Message, Category, Verbosity)

/*
 * 打印日志
 * @param Message 消息内容
 * @param Category 调试类别
 * @param Verbosity 调试级别
 */
FORCEINLINE void WHLog(const FString& Message, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log)
{
 UDebugModuleStatics::LogMessage(Message, Category, Verbosity);
}

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
FORCEINLINE void WHDebug(const FString& Message, EDebugMode Mode = EM_Screen, EDebugCategory Category = EDC_Default, EDebugVerbosity Verbosity = EDV_Log, const FColor& DisplayColor = FColor::Cyan, float Duration = 1.5f, int32 Key = -1, bool bNewerOnTop = true)
{
 UDebugModuleStatics::DebugMessage(Message, Mode, Category, Verbosity, DisplayColor, Duration, Key, bNewerOnTop);
}