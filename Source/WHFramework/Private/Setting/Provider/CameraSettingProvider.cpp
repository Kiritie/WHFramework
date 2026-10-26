#include "Setting/Provider/CameraSettingProvider.h"

#include "Camera/CameraModule.h"
#include "Setting/SettingModule.h"
#include "UObject/UnrealType.h"

UCameraModule* UCameraSettingProvider::GetCameraModule() const
{
	const USettingModule* Owner = GetTypedOuter<USettingModule>();
	return Owner ? UCameraModule::GetPtr(Owner->GetWorld()) : nullptr;
}

void UCameraSettingProvider::ResolveDisplay(const FCameraConfig& InConfig, FName InProperty, float& OutScale, float& OutOffset)
{
	OutScale = 1.f;
	OutOffset = 0.f;
	if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, MovementSensitivity)) OutScale = InConfig.Movement.MoveSpeed;
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, HorizontalLookSensitivity)) OutScale = InConfig.Rotation.HorizontalSpeed;
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, VerticalLookSensitivity)) OutScale = InConfig.Rotation.VerticalSpeed;
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, HorizontalPanSensitivity)) OutScale = InConfig.Movement.HorizontalPanSpeed;
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, VerticalPanSensitivity)) OutScale = InConfig.Movement.VerticalPanSpeed;
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, ZoomSensitivity)) OutScale = InConfig.Zoom.Speed;
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, MovementSmoothing))
	{
		OutOffset = InConfig.Movement.MaxSmoothSpeed;
		OutScale = InConfig.Movement.MinSmoothSpeed - OutOffset;
	}
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, RotationSmoothing))
	{
		OutOffset = InConfig.Rotation.MaxSmoothSpeed;
		OutScale = InConfig.Rotation.MinSmoothSpeed - OutOffset;
	}
	else if (InProperty == GET_MEMBER_NAME_CHECKED(FCameraUserSettings, ZoomSmoothing))
	{
		OutOffset = InConfig.Zoom.MaxSmoothSpeed;
		OutScale = InConfig.Zoom.MinSmoothSpeed - OutOffset;
	}
}

void UCameraSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	const UCameraModule* Camera = GetCameraModule();
	if (!Camera) return;
	const FCameraConfig& CurrentConfig = Camera->GetCameraConfig();
	auto AddDefinition = [&](FName Property, const FText& Name, FName Category, int32 Order, bool bCapability, FName Condition = NAME_None)
	{
		const FProperty* Field = FCameraUserSettings::StaticStruct()->FindPropertyByName(Property);
		FSettingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
		Definition.SettingId = FSettingId(FName(*(TEXT("Camera.") + Property.ToString())));
		Definition.SourcePath = Property.ToString();
		Definition.Provider = FName(TEXT("Camera"));
		Definition.Page = FName(TEXT("Camera"));
		Definition.Category = Category;
		Definition.CategoryDisplayName = Category == FName(TEXT("Camera Move")) ? NSLOCTEXT("WH.CameraSetting", "MoveCategory", "相机移动") :
			Category == FName(TEXT("Camera Rotate")) ? NSLOCTEXT("WH.CameraSetting", "RotateCategory", "相机旋转") : NSLOCTEXT("WH.CameraSetting", "ZoomCategory", "相机缩放");
		Definition.CategoryOrder = Category == FName(TEXT("Camera Move")) ? 0 : Category == FName(TEXT("Camera Rotate")) ? 10 : 20;
		Definition.Order = Order;
		Definition.DisplayName = Name;
		Definition.Renderer = CastField<FBoolProperty>(Field) ? ESettingRendererType::Bool : ESettingRendererType::Number;
		Definition.ApplyPolicy = ESettingApplyPolicy::Deferred;
		Definition.bEnabled = bCapability;
		if (!bCapability) Definition.Description = NSLOCTEXT("WH.CameraSetting", "CapabilityDisabled", "当前相机配置未开放此能力。");
		if (!Condition.IsNone())
		{
			FSettingCondition& Enabled = Definition.EnableConditions.AddDefaulted_GetRef();
			Enabled.OtherSetting = FSettingId(FName(*(TEXT("Camera.") + Condition.ToString())));
			Enabled.Value = FParameter(true);
		}
		if (Definition.Renderer == ESettingRendererType::Number)
		{
			float Scale;
			float Offset;
			ResolveDisplay(CurrentConfig, Property, Scale, Offset);
			const bool bSmoothing = Property.ToString().EndsWith(TEXT("Smoothing"));
			Definition.NumberDisplay.bHasMin = true;
			Definition.NumberDisplay.bHasMax = true;
			Definition.NumberDisplay.Min = bSmoothing ? FMath::Min(Offset, Offset + Scale) : 0.f;
			Definition.NumberDisplay.Max = bSmoothing ? FMath::Max(Offset, Offset + Scale) : FMath::Max(0.f, Scale) * 10.f;
			Definition.NumberDisplay.Step = 0.1;
			Definition.NumberDisplay.DecimalPlaces = 1;
			Definition.NumberDisplay.Suffix = bSmoothing ? NSLOCTEXT("WH.CameraSetting", "PerSecond", " /s") :
				Category == FName(TEXT("Camera Rotate")) ? NSLOCTEXT("WH.CameraSetting", "DegreesPerSecond", " °/s") : NSLOCTEXT("WH.CameraSetting", "CentimetersPerSecond", " cm/s");
			Definition.bEnabled &= !FMath::IsNearlyZero(Scale);
		}
	};
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bEnableVerticalPan), NSLOCTEXT("WH.CameraSetting", "VerticalPan", "垂直平移"), TEXT("Camera Move"), 0, CurrentConfig.Movement.bEnableVerticalPan);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, MovementSensitivity), NSLOCTEXT("WH.CameraSetting", "MoveSpeed", "移动速度"), TEXT("Camera Move"), 10, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, HorizontalPanSensitivity), NSLOCTEXT("WH.CameraSetting", "HorizontalPanSpeed", "水平平移速度"), TEXT("Camera Move"), 20, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, VerticalPanSensitivity), NSLOCTEXT("WH.CameraSetting", "VerticalPanSpeed", "垂直平移速度"), TEXT("Camera Move"), 30, CurrentConfig.Movement.bEnableVerticalPan, TEXT("bEnableVerticalPan"));
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bInvertHorizontalPan), NSLOCTEXT("WH.CameraSetting", "InvertHorizontalPan", "水平平移反转"), TEXT("Camera Move"), 40, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bInvertVerticalPan), NSLOCTEXT("WH.CameraSetting", "InvertVerticalPan", "垂直平移反转"), TEXT("Camera Move"), 50, CurrentConfig.Movement.bEnableVerticalPan, TEXT("bEnableVerticalPan"));
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bSmoothMovement), NSLOCTEXT("WH.CameraSetting", "SmoothMovement", "平滑移动"), TEXT("Camera Move"), 60, CurrentConfig.Movement.bSmoothMovement);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, MovementSmoothing), NSLOCTEXT("WH.CameraSetting", "MovementSmoothSpeed", "移动平滑速度"), TEXT("Camera Move"), 70, CurrentConfig.Movement.bSmoothMovement, TEXT("bSmoothMovement"));
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, HorizontalLookSensitivity), NSLOCTEXT("WH.CameraSetting", "HorizontalLookSpeed", "水平旋转速度"), TEXT("Camera Rotate"), 0, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, VerticalLookSensitivity), NSLOCTEXT("WH.CameraSetting", "VerticalLookSpeed", "垂直旋转速度"), TEXT("Camera Rotate"), 10, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bInvertHorizontalLook), NSLOCTEXT("WH.CameraSetting", "InvertHorizontalLook", "水平旋转反转"), TEXT("Camera Rotate"), 20, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bInvertVerticalLook), NSLOCTEXT("WH.CameraSetting", "InvertVerticalLook", "垂直旋转反转"), TEXT("Camera Rotate"), 30, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bSmoothRotation), NSLOCTEXT("WH.CameraSetting", "SmoothRotation", "平滑旋转"), TEXT("Camera Rotate"), 40, CurrentConfig.Rotation.bSmoothRotation);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, RotationSmoothing), NSLOCTEXT("WH.CameraSetting", "RotationSmoothSpeed", "旋转平滑速度"), TEXT("Camera Rotate"), 50, CurrentConfig.Rotation.bSmoothRotation, TEXT("bSmoothRotation"));
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, ZoomSensitivity), NSLOCTEXT("WH.CameraSetting", "ZoomSpeed", "缩放速度"), TEXT("Camera Zoom"), 0, true);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, bSmoothZoom), NSLOCTEXT("WH.CameraSetting", "SmoothZoom", "平滑缩放"), TEXT("Camera Zoom"), 10, CurrentConfig.Zoom.bSmoothZoom);
	AddDefinition(GET_MEMBER_NAME_CHECKED(FCameraUserSettings, ZoomSmoothing), NSLOCTEXT("WH.CameraSetting", "ZoomSmoothSpeed", "缩放平滑速度"), TEXT("Camera Zoom"), 20, CurrentConfig.Zoom.bSmoothZoom, TEXT("bSmoothZoom"));
}

bool UCameraSettingProvider::CanHandle(const FSettingDefinition& InDefinition) const
{
	return InDefinition.Provider == FName(TEXT("Camera"));
}

void UCameraSettingProvider::BeginEdit(const TArray<FSettingDefinition>& InDefinitions)
{
	Super::BeginEdit(InDefinitions);
	if (const UCameraModule* Camera = GetCameraModule())
	{
		Config = Camera->GetCameraConfig();
		AppliedSettings = Camera->GetUserSettings();
		PendingSettings = AppliedSettings;
		DefaultSettings = FCameraUserSettings();
	}
}

FParameter UCameraSettingProvider::ReadValue(const FCameraUserSettings& InSettings, const FSettingDefinition& InDefinition) const
{
	const FName Name(*InDefinition.SourcePath);
	const FProperty* Property = FCameraUserSettings::StaticStruct()->FindPropertyByName(Name);
	if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property)) return FParameter(Bool->GetPropertyValue_InContainer(&InSettings));
	if (const FFloatProperty* Number = CastField<FFloatProperty>(Property))
	{
		float Scale;
		float Offset;
		ResolveDisplay(Config, Name, Scale, Offset);
		return FParameter(Number->GetPropertyValue_InContainer(&InSettings) * Scale + Offset);
	}
	return FParameter();
}

bool UCameraSettingProvider::WriteValue(FCameraUserSettings& InOutSettings, const FSettingDefinition& InDefinition, const FParameter& InValue) const
{
	const FName Name(*InDefinition.SourcePath);
	FProperty* Property = FCameraUserSettings::StaticStruct()->FindPropertyByName(Name);
	if (FBoolProperty* Bool = CastField<FBoolProperty>(Property); Bool && InValue.Is<bool>())
	{
		Bool->SetPropertyValue_InContainer(&InOutSettings, InValue.Get<bool>());
		return true;
	}
	if (FFloatProperty* Number = CastField<FFloatProperty>(Property); Number && InValue.Is<float>())
	{
		float Scale;
		float Offset;
		ResolveDisplay(Config, Name, Scale, Offset);
		if (!FMath::IsFinite(InValue.Get<float>()) || FMath::IsNearlyZero(Scale)) return false;
		Number->SetPropertyValue_InContainer(&InOutSettings, (InValue.Get<float>() - Offset) / Scale);
		return true;
	}
	return false;
}

FParameter UCameraSettingProvider::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	return ReadValue(AppliedSettings, InDefinition);
}

FParameter UCameraSettingProvider::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	return ReadValue(PendingSettings, InDefinition);
}

FParameter UCameraSettingProvider::GetDefaultValue(const FSettingDefinition& InDefinition) const
{
	return ReadValue(DefaultSettings, InDefinition);
}

FSettingValidationResult UCameraSettingProvider::Validate(const FSettingDefinition& InDefinition, const FParameter& InValue) const
{
	if (const float* Number = InValue.GetPtr<float>(); Number && !FMath::IsFinite(*Number))
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.CameraSetting", "NotFinite", "相机参数必须为有限数值。"));
	}
	return Super::Validate(InDefinition, InValue);
}

bool UCameraSettingProvider::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	return CanHandle(InDefinition) && WriteValue(PendingSettings, InDefinition, InValue);
}

bool UCameraSettingProvider::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	UCameraModule* Camera = GetCameraModule();
	if (!Camera || !CanHandle(InDefinition)) return false;
	FCameraUserSettings Current = Camera->GetUserSettings();
	if (!WriteValue(Current, InDefinition, InValue)) return false;
	Camera->ApplyUserSettings(Current);
	return true;
}

void UCameraSettingProvider::Rollback(const FSettingDefinition& InDefinition)
{
	const FParameter Value = ReadValue(AppliedSettings, InDefinition);
	Apply(InDefinition, Value);
	WriteValue(PendingSettings, InDefinition, Value);
}

void UCameraSettingProvider::Commit()
{
	AppliedSettings = PendingSettings;
}

void UCameraSettingProvider::EndEdit()
{
	AppliedSettings = FCameraUserSettings();
	PendingSettings = FCameraUserSettings();
	DefaultSettings = FCameraUserSettings();
	Config = FCameraConfig();
}
