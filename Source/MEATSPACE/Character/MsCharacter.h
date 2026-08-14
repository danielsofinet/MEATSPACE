#pragma once

#include "CoreMinimal.h"
#include "Combat/MsCombatTypes.h"
#include "GameFramework/Character.h"
#include "MsCharacter.generated.h"

class UMsWeaponComponent;
class UMsMeleeComponent;

/**
 * Base player character for MEATSPACE.
 *
 * Owns both weapons at once and routes attack input to whichever is equipped. Keeping both
 * components always-present (rather than spawning/destroying on swap) is what makes switching
 * instant - swapping only changes which one listens.
 */
UCLASS()
class MEATSPACE_API AMsCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMsCharacter();

	/** Anti-air. Hitscan. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	UMsWeaponComponent* GetWeapon() const { return Weapon; }

	/** Anti-ground. Swept arc. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	UMsMeleeComponent* GetMelee() const { return Melee; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	EMsWeaponSlot GetActiveSlot() const { return ActiveSlot; }

	/** Swap weapons. Safe to call on client - replicates to the server. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Combat")
	void EquipSlot(EMsWeaponSlot NewSlot);

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Combat")
	TObjectPtr<UMsWeaponComponent> Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Combat")
	TObjectPtr<UMsMeleeComponent> Melee;

	/** Which weapon is out. Replicated so other players can see it later. */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlot, BlueprintReadOnly, Category = "Meatspace|Combat")
	EMsWeaponSlot ActiveSlot = EMsWeaponSlot::Gun;

	/** Which weapon the character starts the game holding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Combat")
	EMsWeaponSlot StartingSlot = EMsWeaponSlot::Gun;

	/**
	 * Offsets the camera boom over the character's shoulder.
	 *
	 * The crosshair marks the centre of the screen because that is genuinely where shots go.
	 * With a centred camera the character stands right on top of that point, so you are aiming
	 * at your own back. Shifting the camera sideways clears the line of sight without changing
	 * where the gun actually shoots.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	bool bUseShoulderCamera = true;

	/** X = forward, Y = right, Z = up. Raise Y to push the character further off-centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Camera")
	FVector ShoulderOffset = FVector(0.0f, 75.0f, 45.0f);

	UFUNCTION()
	void OnRep_ActiveSlot();

	UFUNCTION(Server, Reliable)
	void ServerEquipSlot(EMsWeaponSlot NewSlot);

	virtual void BeginPlay() override;

private:
	void OnAttackPressed();
	void OnAttackReleased();
	void OnSelectSword();
	void OnSelectGun();

	/** On-screen readout of the current weapon. Debug only. */
	void ShowWeaponFeedback() const;
};
