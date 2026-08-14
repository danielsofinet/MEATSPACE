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

	// --- Health bar. Deliberately wordless: no text means no localization burden, and a bar
	// reads faster than a number in a fight anyway. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health")
	bool bShowHealthBar = true;

	/** Bar width as a fraction of screen width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HealthBarWidthFraction = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health", meta = (ClampMin = "2.0"))
	float HealthBarHeight = 16.0f;

	/** Distance from the bottom of the screen, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health", meta = (ClampMin = "0.0"))
	float HealthBarBottomMargin = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health")
	FLinearColor HealthBarBackColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.75f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health")
	FLinearColor HealthHighColor = FLinearColor(0.25f, 0.9f, 0.45f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health")
	FLinearColor HealthLowColor = FLinearColor(0.95f, 0.2f, 0.15f, 0.95f);

	/** Peak opacity of the red flash when hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageFlashOpacity = 0.30f;

	void DrawHealthBar(const class AMsCharacter* Character);
	void DrawDamageFlash(const class AMsCharacter* Character);
};
