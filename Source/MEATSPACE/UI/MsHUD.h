#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MsHUD.generated.h"

/**
 * Minimal crosshair.
 *
 * Drawn in code rather than as a UMG widget on purpose: a widget would be a binary asset
 * Claude cannot author, and this needs to exist before we know what the real HUD looks like.
 * When MEATSPACE gets a proper HUD - health, ammo, district progress - that becomes UMG and
 * this goes away.
 *
 * Note the crosshair marks where the CAMERA is aiming, which is exactly what the gun traces
 * along, so it tells the truth about where shots go.
 */
UCLASS()
class MEATSPACE_API AMsHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	/** Size of the centre dot in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Crosshair", meta = (ClampMin = "1.0"))
	float DotSize = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Crosshair")
	FLinearColor DotColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	/** Dark border behind the dot so it stays readable against bright surfaces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Crosshair", meta = (ClampMin = "0.0"))
	float OutlineThickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Crosshair")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/** Show the crosshair while the sword is out. It still marks your facing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Crosshair")
	bool bShowWithSword = true;

	/** Dot colour while the sword is equipped, so the two modes read differently at a glance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Crosshair")
	FLinearColor SwordDotColor = FLinearColor(0.35f, 0.85f, 1.0f, 0.7f);
};
