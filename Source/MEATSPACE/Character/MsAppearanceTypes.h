#pragma once

#include "CoreMinimal.h"
#include "MsAppearanceTypes.generated.h"

/**
 * Where a piece of clothing goes, and which patch of skin it covers.
 *
 * Each slot maps to a material section on the body mesh. Equipping a garment hides that
 * section, which is how clipping is solved: a jacket does not fight with the torso, it
 * replaces it.
 */
UENUM(BlueprintType)
enum class EMsBodySlot : uint8
{
	Head	UMETA(DisplayName = "Head"),
	Torso	UMETA(DisplayName = "Torso"),
	Arms	UMETA(DisplayName = "Arms"),
	Hands	UMETA(DisplayName = "Hands"),
	Legs	UMETA(DisplayName = "Legs"),
	Feet	UMETA(DisplayName = "Feet"),

	/** Packs, capes, sheaths. Covers no skin, so nothing is hidden for it. */
	Back	UMETA(DisplayName = "Back"),

	MAX		UMETA(Hidden)
};

/**
 * Everything that makes one character look like themselves.
 *
 * Kept as a single replicated struct rather than loose properties so co-op peers receive a
 * coherent appearance in one update - a half-applied character (new jacket, old skin tone)
 * would be visible for a frame otherwise.
 */
USTRUCT(BlueprintType)
struct FMsAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor SkinTone = FLinearColor(0.76f, 0.57f, 0.44f, 1.0f);

	/** One entry per EMsBodySlot. Null means nothing equipped, showing bare skin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TArray<TObjectPtr<USkeletalMesh>> Garments;

	/**
	 * Colourway per slot, applied to every material on that garment.
	 *
	 * White leaves the garment's own material untouched, so a piece can either be tinted or
	 * ship with its own authored look. That matters for cosmetics: most items want to be
	 * recolourable, but a few special ones want to be exactly what the artist made.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TArray<FLinearColor> GarmentTints;

	/** Which body mesh to use. Body type is a mesh swap, not a morph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<USkeletalMesh> BodyMesh = nullptr;
};
