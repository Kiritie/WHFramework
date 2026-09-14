#include "Step/Customization/StepCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Step/Base/StepBase.h"
FStepCustomization::FStepCustomization() { }
TSharedRef<IDetailCustomization> FStepCustomization::MakeInstance() { return MakeShared<FStepCustomization>(); }
void FStepCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	IDetailCategoryBuilder& Category = DetailLayoutBuilder.EditCategory(TEXT("Camera"));
	Category.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, bApplyCameraAction)));
	Category.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, CameraModeClass)));
	Category.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, CameraOverride)));
	Category.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, CameraAnchor)));
	Category.AddProperty(DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStepBase, CameraTransition)));
}
