#include "Character/MsAppearanceComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Rendering/SkeletalMeshRenderData.h"

UMsAppearanceComponent::UMsAppearanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Defaults match the material slot names specified in ASSET_REQUESTS.md. Back covers no
	// skin, so it deliberately has no entry.
	SkinSectionNames.Add(EMsBodySlot::Head, FName("Skin_Head"));
	SkinSectionNames.Add(EMsBodySlot::Torso, FName("Skin_Torso"));
	SkinSectionNames.Add(EMsBodySlot::Arms, FName("Skin_Arms"));
	SkinSectionNames.Add(EMsBodySlot::Hands, FName("Skin_Hands"));
	SkinSectionNames.Add(EMsBodySlot::Legs, FName("Skin_Legs"));
	SkinSectionNames.Add(EMsBodySlot::Feet, FName("Skin_Feet"));

	DefaultAppearance.Garments.SetNum(static_cast<int32>(EMsBodySlot::MAX));

	// White is "no tint" - the garment keeps whatever look its material was authored with.
	DefaultAppearance.GarmentTints.Init(FLinearColor::White, static_cast<int32>(EMsBodySlot::MAX));
}

void UMsAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMsAppearanceComponent, Appearance);
}

void UMsAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();

	// The server owns the appearance; clients receive it and apply it through OnRep. Applying
	// the default locally as well means a listen-server host looks right immediately.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Appearance = DefaultAppearance;
	}

	EnsureSlotArrays();

	ApplyAppearance();
}

void UMsAppearanceComponent::EnsureSlotArrays()
{
	const int32 SlotCount = static_cast<int32>(EMsBodySlot::MAX);

	Appearance.Garments.SetNum(SlotCount);

	const int32 FirstNewTint = Appearance.GarmentTints.Num();
	if (FirstNewTint < SlotCount)
	{
		Appearance.GarmentTints.SetNum(SlotCount);
		for (int32 TintIndex = FirstNewTint; TintIndex < SlotCount; ++TintIndex)
		{
			Appearance.GarmentTints[TintIndex] = FLinearColor::White;
		}
	}
}

USkeletalMeshComponent* UMsAppearanceComponent::GetBodyMesh() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UMsAppearanceComponent::OnRep_Appearance()
{
	ApplyAppearance();
}

void UMsAppearanceComponent::SetAppearance(const FMsAppearance& NewAppearance)
{
	Appearance = NewAppearance;
	EnsureSlotArrays();

	ApplyAppearance();
}

void UMsAppearanceComponent::SetSkinTone(FLinearColor NewTone)
{
	Appearance.SkinTone = NewTone;
	ApplySkinTone();
}

USkeletalMesh* UMsAppearanceComponent::GetEquippedGarment(EMsBodySlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Appearance.Garments.IsValidIndex(Index) ? Appearance.Garments[Index] : nullptr;
}

void UMsAppearanceComponent::EquipGarment(EMsBodySlot Slot, USkeletalMesh* GarmentMesh)
{
	EnsureSlotArrays();

	Appearance.Garments[static_cast<int32>(Slot)] = GarmentMesh;

	ApplyGarments();
}

void UMsAppearanceComponent::UnequipGarment(EMsBodySlot Slot)
{
	EquipGarment(Slot, nullptr);
}

void UMsAppearanceComponent::SetGarmentTint(EMsBodySlot Slot, FLinearColor Tint)
{
	EnsureSlotArrays();

	Appearance.GarmentTints[static_cast<int32>(Slot)] = Tint;

	ApplyGarments();
}

void UMsAppearanceComponent::ApplyAppearance()
{
	USkeletalMeshComponent* Body = GetBodyMesh();
	if (!Body)
	{
		return;
	}

	// Body type is a mesh swap. Both bodies share a skeleton, so animation is unaffected.
	if (Appearance.BodyMesh && Body->GetSkeletalMeshAsset() != Appearance.BodyMesh)
	{
		Body->SetSkeletalMeshAsset(Appearance.BodyMesh);
	}

	ApplySkinTone();
	ApplyGarments();
}

void UMsAppearanceComponent::ApplySkinTone()
{
	USkeletalMeshComponent* Body = GetBodyMesh();
	if (!Body)
	{
		return;
	}

	// Every section gets the same tone. They are separate sections only so clothing can hide
	// them individually - to the player they must read as one continuous skin.
	const int32 NumMaterials = Body->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		if (UMaterialInstanceDynamic* Dynamic = Body->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
		{
			Dynamic->SetVectorParameterValue(SkinToneParameter, Appearance.SkinTone);
		}
	}
}

void UMsAppearanceComponent::ApplyGarments()
{
	USkeletalMeshComponent* Body = GetBodyMesh();
	if (!Body)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EMsBodySlot::MAX); ++SlotIndex)
	{
		const EMsBodySlot Slot = static_cast<EMsBodySlot>(SlotIndex);
		USkeletalMesh* GarmentMesh = Appearance.Garments.IsValidIndex(SlotIndex)
			? Appearance.Garments[SlotIndex].Get()
			: nullptr;

		TObjectPtr<USkeletalMeshComponent>* ExistingComponent = GarmentComponents.Find(Slot);

		if (GarmentMesh)
		{
			USkeletalMeshComponent* GarmentComponent = ExistingComponent ? ExistingComponent->Get() : nullptr;

			if (!GarmentComponent)
			{
				GarmentComponent = NewObject<USkeletalMeshComponent>(GetOwner());
				GarmentComponent->SetupAttachment(Body);
				GarmentComponent->RegisterComponent();

				// Driven entirely by the body's pose. No animation blueprint, no separate
				// evaluation - this is what makes a garment cost almost nothing and animate
				// perfectly in step with the body.
				GarmentComponent->SetLeaderPoseComponent(Body);

				GarmentComponents.Add(Slot, GarmentComponent);
			}

			GarmentComponent->SetSkeletalMeshAsset(GarmentMesh);
			GarmentComponent->SetVisibility(true);

			// Colourway. Applied to every material on the garment, so a piece made of several
			// materials still recolours as one item.
			const FLinearColor Tint = Appearance.GarmentTints.IsValidIndex(SlotIndex)
				? Appearance.GarmentTints[SlotIndex]
				: FLinearColor::White;

			const int32 NumGarmentMaterials = GarmentComponent->GetNumMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < NumGarmentMaterials; ++MaterialIndex)
			{
				if (UMaterialInstanceDynamic* Dynamic = GarmentComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
				{
					Dynamic->SetVectorParameterValue(GarmentTintParameter, Tint);
				}
			}

			// Hide the skin underneath so it cannot poke through.
			if (const FName* SectionName = SkinSectionNames.Find(Slot))
			{
				SetSkinSectionVisible(*SectionName, false);
			}
		}
		else
		{
			if (ExistingComponent && ExistingComponent->Get())
			{
				ExistingComponent->Get()->DestroyComponent();
			}
			GarmentComponents.Remove(Slot);

			// Nothing covering it any more, so the skin comes back.
			if (const FName* SectionName = SkinSectionNames.Find(Slot))
			{
				SetSkinSectionVisible(*SectionName, true);
			}
		}
	}
}

void UMsAppearanceComponent::SetSkinSectionVisible(FName MaterialSlotName, bool bVisible)
{
	USkeletalMeshComponent* Body = GetBodyMesh();
	if (!Body || MaterialSlotName.IsNone())
	{
		return;
	}

	const int32 MaterialIndex = Body->GetMaterialIndex(MaterialSlotName);
	if (MaterialIndex == INDEX_NONE)
	{
		// The mesh does not have this section. Usually means the material slot names in the
		// modelling package do not match SkinSectionNames.
		return;
	}

	USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
	if (!Mesh)
	{
		return;
	}

	FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
	if (!RenderData)
	{
		return;
	}

	// A material can span several render sections, and section indices are not material
	// indices - so walk the sections and hide the ones using this material, per LOD.
	for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
	{
		const TArray<FSkelMeshRenderSection>& Sections = RenderData->LODRenderData[LODIndex].RenderSections;

		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			if (Sections[SectionIndex].MaterialIndex == MaterialIndex)
			{
				Body->ShowMaterialSection(MaterialIndex, SectionIndex, bVisible, LODIndex);
			}
		}
	}
}
