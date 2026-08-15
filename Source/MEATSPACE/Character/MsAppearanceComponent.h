#pragma once

#include "Character/MsAppearanceTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MsAppearanceComponent.generated.h"

class USkeletalMeshComponent;

/**
 * Modular character appearance: body, skin tone and clothing slots.
 *
 * How it works:
 *   - The body is one skeletal mesh split into material sections (Skin_Head, Skin_Torso, ...)
 *   - Each garment is its own skeletal mesh on the SAME skeleton, attached to the body and
 *     driven by it, so it animates from the body's bones with no extra rigging
 *   - Equipping a garment hides the skin section it covers, which is how clipping is avoided
 *   - Skin tone is a material parameter pushed to every section at once, so one mesh serves
 *     every skin tone with no extra art
 *
 * Body type is a mesh swap rather than a morph, because two bodies built on the same skeleton
 * share every animation and, if authored together, share UVs and therefore every texture.
 */
UCLASS(ClassGroup = (Meatspace), meta = (BlueprintSpawnableComponent))
class MEATSPACE_API UMsAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMsAppearanceComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Applies an entire appearance at once. Server-side; replicates to everyone. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Appearance")
	void SetAppearance(const FMsAppearance& NewAppearance);

	UFUNCTION(BlueprintCallable, Category = "Meatspace|Appearance")
	void SetSkinTone(FLinearColor NewTone);

	/** Pass null to strip the slot back to bare skin. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Appearance")
	void EquipGarment(EMsBodySlot Slot, USkeletalMesh* GarmentMesh);

	UFUNCTION(BlueprintCallable, Category = "Meatspace|Appearance")
	void UnequipGarment(EMsBodySlot Slot);

	UFUNCTION(BlueprintPure, Category = "Meatspace|Appearance")
	USkeletalMesh* GetEquippedGarment(EMsBodySlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Meatspace|Appearance")
	const FMsAppearance& GetAppearance() const { return Appearance; }

protected:
	virtual void BeginPlay() override;

	/**
	 * Material slot name on the body mesh for each clothing slot. Must match the names used
	 * in the modelling package - they are how a garment finds the skin it hides.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Appearance")
	TMap<EMsBodySlot, FName> SkinSectionNames;

	/** Vector parameter on the skin material that receives the tone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Appearance")
	FName SkinToneParameter = FName("SkinTone");

	/** Starting look, applied on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Appearance")
	FMsAppearance DefaultAppearance;

	UPROPERTY(ReplicatedUsing = OnRep_Appearance)
	FMsAppearance Appearance;

	UFUNCTION()
	void OnRep_Appearance();

	/** Rebuilds everything from Appearance. Safe to call repeatedly. */
	void ApplyAppearance();

	void ApplySkinTone();
	void ApplyGarments();

	/** Shows or hides every render section using the named material slot. */
	void SetSkinSectionVisible(FName MaterialSlotName, bool bVisible);

	/** The owning actor's skeletal mesh - the body everything else hangs off. */
	USkeletalMeshComponent* GetBodyMesh() const;

private:
	/** One spawned component per occupied slot, driven by the body's pose. */
	UPROPERTY(Transient)
	TMap<EMsBodySlot, TObjectPtr<USkeletalMeshComponent>> GarmentComponents;
};
