/**
 * @file	io_pins.cpp
 * @brief	his file is about general input/output utility methods, not much EFI-specifics
 *
 *
 * @date Jan 24, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"

#if EFI_PROD_CODE

#include "drivers/gpio/gpio_ext.h"

#include "status_loop.h"
#include "console_io.h"
#endif /* EFI_PROD_CODE */

void efiSetPadUnused(brain_pin_e brainPin) {
#if EFI_PROD_CODE
	/* input with pull up, is it safe? */
	iomode_t mode = PAL_STM32_MODE_INPUT | PAL_STM32_PUPDR_PULLUP;

	if (brain_pin_is_onchip(brainPin)) {
		ioportid_t port = getHwPort("unused", brainPin);
		ioportmask_t pin = getHwPin("unused", brainPin);

		/* input with pull up, is it safe?
		 * todo: shall we reuse 'default state' constants with board.h?
		 * */
		palSetPadMode(port, pin, mode);
		palWritePad(port, pin, 0);
	}
	#if (BOARD_EXT_GPIOCHIPS > 0)
		else {
			gpiochips_setPadMode(brainPin, mode);
		}
	#endif
#endif /* EFI_PROD_CODE */

	brain_pin_markUnused(brainPin);
}

/**
 * This method would set an error condition if pin is already used
 */
void efiSetPadMode(const char *msg, brain_pin_e brainPin, iomode_t mode) {
	if (!isBrainPinValid(brainPin)) {
		// No pin configured, nothing to do here.
		return;
	}

	bool wasUsed = brain_pin_markUsed(brainPin, msg);

	if (!wasUsed) {
		efiSetPadModeWithoutOwnershipAcquisition(msg, brainPin, mode);
	}
}

void efiSetPadModeWithoutOwnershipAcquisition(const char *msg, brain_pin_e brainPin, iomode_t mode) {
#if EFI_PROD_CODE
	/*check if on-chip pin or external */
	if (brain_pin_is_onchip(brainPin)) {
		/* on-chip */
		ioportid_t port = getHwPort(msg, brainPin);
		ioportmask_t pin = getHwPin(msg, brainPin);
		/* paranoid */
		if (!port)
			return;

		palSetPadMode(port, pin, mode);
	}
	#if (BOARD_EXT_GPIOCHIPS > 0)
		else {
			gpiochips_setPadMode(brainPin, mode);
		}
	#endif

#endif /* EFI_PROD_CODE */
}

#if EFI_PROD_CODE

#if EFI_ENGINE_CONTROL
#include "main_trigger_callback.h"
#endif /* EFI_ENGINE_CONTROL */

class CanInputPin final {
public:
	bool get(float timeoutSec, bool defaultValue) const {
		if (m_timeout.hasElapsedSec(timeoutSec)) {
			return defaultValue;
		}

		return m_value;
	}

	void set(bool value) {
		m_value = value;
		m_timeout.reset();
	}

private:
	// state
	Timer m_timeout;
	bool m_value;
};

static CanInputPin canVirtualInputs[CAN_VIRTUAL_INPUT_PINS_COUNT];

void setCanVirtualInput(size_t idx, bool value) {
	if (idx >= CAN_VIRTUAL_INPUT_PINS_COUNT) {
		firmwareError(ObdCode::OBD_PCM_Processor_Fault, "invalid setCanVirtualInput index %u", idx);
		return;
	}

	canVirtualInputs[idx].set(value);
}

static bool readCanVirtualInput(size_t idx) {
	if (idx >= CAN_VIRTUAL_INPUT_PINS_COUNT) {
		firmwareError(ObdCode::OBD_PCM_Processor_Fault, "invalid readCanVirtualInput index %u", idx);
		return false;
	}

	const auto& inputConf = engineConfiguration->canVirtualInputs[idx];

	return canVirtualInputs[idx].get(
		inputConf.timeout,
		inputConf.defaultValue != 0
	);
}

bool efiReadPin(Gpio pin) {
	if (!isBrainPinValid(pin)) {
		return false;
	}

	if (brain_pin_is_onchip(pin)) {
		return palReadPad(getHwPort("readPin", pin), getHwPin("readPin", pin));
	}

	#if (BOARD_EXT_GPIOCHIPS > 0)
		if (brain_pin_is_ext(pin)) {
			return (gpiochips_readPad(pin) > 0);
		}
	#endif

	if (pin >= Gpio::CAN_INPUT_0 && pin <= Gpio::CAN_INPUT_7) {
		return readCanVirtualInput(pin - Gpio::CAN_INPUT_0);
	}

	/* incorrect pin */
	return false;
}

iomode_t getInputMode(pin_input_mode_e mode) {
	switch (mode) {
	case PI_PULLUP:
		return PAL_MODE_INPUT_PULLUP;
	case PI_PULLDOWN:
		return PAL_MODE_INPUT_PULLDOWN;
	case PI_DEFAULT:
	default:
		return PAL_MODE_INPUT;
	}
}

#else /* EFI_PROD_CODE */

// This has been made global so we don't need to worry about efiReadPin having access the object
//  we store it in, every time we need to use efiReadPin.
bool mockPinStates[BRAIN_PIN_COUNT];

bool efiReadPin(brain_pin_e pin) {
	return mockPinStates[static_cast<int>(pin)];
}

void setMockState(brain_pin_e pin, bool state) {
	mockPinStates[static_cast<int>(pin)] = state;
}

#endif /* EFI_PROD_CODE */
