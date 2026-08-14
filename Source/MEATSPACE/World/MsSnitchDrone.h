#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsSnitchDrone.generated.h"

class AMsEncounterVolume;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnSnitched, class AMsSnitchDrone*, Drone);

/**
 * A delivery drone going about its business, until it notices you are carrying a sword.
 *
 * This is the onboarding's inciting incident, and it exists to make the first fight make
 * sense: you are not attacked for walking down a street, you are reported for being armed.
 * The player did something (took grandpa's sword) and the world reacted.
 *
 * Mechanically it is a delayed trigger, but the delay is the point - the gap between it
 * noticing you and the dropships arriving is where the player understands cause and effect.
 */
UCLASS()
class MEATSPACE_API AMsSnitchDrone : public AActor
{
	GENERATED_BODY()

public:
	AMsSnitchDrone();

	virtual void Tick(float DeltaSeconds) override;

	/** Reports the player regardless of proximity. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Drone")
	void Snitch();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Drone")
	bool HasSnitched() const { return bSnitched; }

	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Drone")
	FMsOnSnitched OnSnitched;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Drone")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How close the player has to get to be noticed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone", meta = (ClampMin = "50.0"))
	float NoticeRadius = 900.0f;

	/**
	 * Only reacts to an armed player. The whole premise is that the sword is the problem, so
	 * an unarmed player should be able to walk straight past.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone")
	bool bOnlyIfArmed = true;

	/** Beat between noticing you and the encounter starting - the alarm going out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone", meta = (ClampMin = "0.0"))
	float AlertDelay = 2.0f;

	/**
	 * What it says. Placeholder for a spoken bark - shown on screen for now so the beat can
	 * be tested before there is audio.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone", meta = (MultiLine = "true"))
	FText AlertLine;

	/** The fight this drone calls in. Pick the encounter volume in the level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Meatspace|Drone")
	TObjectPtr<AMsEncounterVolume> EncounterToCall;

	/** Bobs in place so it reads as hovering rather than parked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone", meta = (ClampMin = "0.0"))
	float HoverBobAmplitude = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone", meta = (ClampMin = "0.0"))
	float HoverBobFrequency = 1.6f;

	/** Flees upward after reporting, so it is not left hanging around during the fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone")
	bool bFleeAfterSnitching = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Drone", meta = (ClampMin = "0.0"))
	float FleeSpeed = 700.0f;

	void CallEncounter();

private:
	bool bSnitched = false;
	bool bFleeing = false;
	float BaseZ = 0.0f;
	float BobTime = 0.0f;

	FTimerHandle AlertTimer;
};
