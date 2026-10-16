# Highload coefficient calculator

## Description

This program can caluclate "A" coefficient of the live-time correction in SpectraLine. This correction is needed for spectra with highload, because of such spectra contain errors due to the random summation of impulses in the analyzer.

## Usage

- Perform several (at least 2) measurements of radioactive source of gamma-rays at the same distance, but with different loading. The different loading can be obtained by another source (with another radionuclide): either more active one, or closer to the detector.
- Write loading values (the spectrum integral countrate) and the measured peak count rates to the table. The loading value can be seen in the lower left corner of the spectrum window in SpectraLine. The peak count rate can be obtained from the peak properties.
- Press the "Calculate" button and the calculated values of the "A" coefficient and its uncertainty will be displayed in the corresponding text fields.
- Copy these values to the SpectraLine configuration parameters.

**Optional**:
- The input data can be saved to a csv file, for example, to process it later.
