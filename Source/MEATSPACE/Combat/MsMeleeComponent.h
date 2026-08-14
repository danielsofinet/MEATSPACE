#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MsMeleeComponent.generated.h"

class UAnimMontage;

/** Where a swing currently is in its lifecycle. */
UENUM(BlueprintType)
enum class EMsSwingPhase : uint8
{
	/** Not swinging. */
	Idle,

	/** Wind-up. Committed, but the blade is not dangerous yet. */
	Windup,

	/** The blade is live and sweeping. This is the only phase that deals damage. */
	Active,

	/** Follow-through. Cannot swing again until this ends. */
	Recovery
};

/**
 * Sword. Swept-arc melee, server-authoritative.
 *
 * Why this is not just "a shorter gun trace": a sword hit is a volume moving through space
 * over time. Each Active tick sweeps a sphere from where the blade was to where it is now,
 * so a fast swing cannot tunnel past a clanker between frames. Each actor can only be hit
 * once per swing, which is what makes cleaving through a crowd feel fair rather than
 * accidentally dealing damage several times to the same target.
 *
 * Same netcode shape as the gun: the swinging client runs the whole state machine locally
 * for feel, but only the server's copy applies damage.
 */
UCLASS(ClassGroup = (Meatspace), meta = (BlueprintSpawnableComponent))
class MEATSPACE_API UMsMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMsMeleeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Begin a swing, if not already mid-swing. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Melee")
	void StartSwing();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Melee")
	bool IsSwinging() const { return Phase != EMsSwingPhase::Idle; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Melee")
	EMsSwingPhase GetPhase() const { return Phase; }

	// --- Tunables. All live-editable; go find what feels good. ---

	/** Damage per target hit. Applies once per actor per swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee")
	float Damage = 45.0f;

	/** How far the blade reaches, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee", meta = (ClampMin = "10.0"))
	float Reach = 220.0f;

	/** Blade thickness. Bigger = more forgiving hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee", meta = (ClampMin = "1.0"))
	float BladeRadius = 35.0f;

	/** Total sweep angle. 140 means 70 degrees either side of forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float ArcDegrees = 140.0f;

	/** Commit delay before the blade goes live. Higher = heavier, more readable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Timing", meta = (ClampMin = "0.0"))
	float WindupTime = 0.12f;

	/** How long the blade is dangerous. This is the swing itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Timing", meta = (ClampMin = "0.01"))
	float ActiveTime = 0.18f;

	/** Follow-through before you can swing again. This governs attack spam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Timing", meta = (ClampMin = "0.0"))
	float RecoveryTime = 0.28f;

	/** Height of the swing plane above the character's origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee")
	float HeightOffset = 60.0f;

	/** Draw the arc so you can see exactly what the blade swept. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Debug")
	bool bDrawDebugSwing = true;

	/**
	 * Optional swing animation. Assign one of the template's melee montages here in the
	 * Blueprint (MM_Attack_01 / _02 / _03 live in Characters/Mannequins/Anims/Unarmed/Attack).
	 * Purely cosmetic - the hit detection above does not depend on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee")
	TObjectPtr<UAnimMontage> SwingMontage;

protected:
	/** Client -> server: "I swung." Server runs its own authoritative swing. */
	UFUNCTION(Server, Reliable)
	void ServerSwing();

	/** Starts the state machine. Runs on both the local client and the server. */
	void BeginSwing();

	/** Sweeps the blade from one point in the arc to the next. */
	void SweepSegment(float PrevAlpha, float NewAlpha);

	/** Blade tip position at a given point through the swing (0 = start, 1 = end). */
	FVector GetBladePoint(float Alpha) const;

	void PlaySwingMontage();

private:
	EMsSwingPhase Phase = EMsSwingPhase::Idle;

	/** Seconds spent in the current phase. */
	float PhaseTime = 0.0f;

	/** Progress through the arc during Active, 0..1. */
	float SwingAlpha = 0.0f;

	/** One hit per actor per swing. */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
};
