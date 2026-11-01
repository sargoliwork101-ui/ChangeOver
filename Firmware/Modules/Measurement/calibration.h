/*
 * ============================================================================
 * @file    calibration.h
 * @brief   [EN] ChangeOver compile-time calibration constants (ADC reference).
 * ----------------------------------------------------------------------------
 * [EN] (2026-10-10) The battery CURRENT tables are NO LONGER compiled in. They are made on
 *      the panel from the bench data and stored in flash (CalLut). Until the
 *      first calibration the board reads through the gain and offset in the
 *      BSP, which start at the MAXIMUM gain and zero offset (safe direction:
 *      a high reading only slows the charge).
 * ============================================================================
 */
#ifndef CALIBRATION_H
#define CALIBRATION_H
/*
 * ============================================================================
 * [EN] ADC REFERENCE (VDDA) CALIBRATION - the single global scale
 *      Every voltage and every current is counts x VDDA / 4095, so VDDA is the
 *      one term common to all of them. The earlier VDDA-3261 mV note is NOT
 *      supported by Calibration Data/solo2_dense.csv (removed, see git history) (the two voltage channels disagree:
 *      +1.2 % and -0.05 %). Treat VDDA as unverified until a bench run with
 *      a reference shows one shared error.
 *      If the error is a GAIN (hypothesis), that would explain why the board
 *      "never calibrates": the error is a GAIN, and the only runtime calibration the product exposes
 *      (params 4/5/6) is an ADDER, which can only be right at one point.
 *      HOW TO CALIBRATE THIS BOARD, once:
 *        1. Flash, then read UINT32_T__G__MeasVddaMv (it is published even
 *           while tracking is off - same watch window as MeasBattery24Mv).
 *        2. Put a DMM on VDDA / the 3.3 V rail.
 *        3. CAL_VREFINT_MV = 1200 * (DMM_VDDA_mV / MeasVddaMv_reading).
 *        4. Set CAL_VDDA_TRACKING_ENABLE to 1, rebuild, re-check all three
 *           voltages against the DMM.
 *      After that the board follows supply and temperature drift by itself.
 *      WHY TRACKING SHIPS OFF: the STM32F103 stores no factory VREFINT
 *      calibration, so before step 3 the 1.20 V is only guaranteed to
 *      1.16..1.24 V (+-3.3 percent) - worse than the error being corrected.
 *      Turning it on un-calibrated would also make every reading LOWER, which
 *      moves the over-voltage cut LATER. Reading high is the safe direction.
 * ============================================================================
 */
#define CAL_VREFINT_MV                 1200u
#define CAL_VDDA_TRACKING_ENABLE          0u
#endif /* CALIBRATION_H */
