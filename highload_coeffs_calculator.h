#pragma once
#ifndef HIGHLOADCOEFFSCALCULATOR_H
#define HIGHLOADCOEFFSCALCULATOR_H

#include <vector>

using Matrix = std::vector<std::vector<double>>;

struct CalculationResults {
    double coeff = 0;
    double d_coeff = 0;
    double chi_2 = 0;
    double cps0 = 0.0;
};


// Calculates highload coeff and unc,
// expects matrix with rows:
// [load, peak_cps, dpeak_cps]
CalculationResults calculateHighLoadCoeffs(const Matrix& matrix);

// Calculates corrected peak count rate to load
// params:
// cps - peak count rate
// coeff - highload coeff
// load - load
// returns:
// corrected peak count rate
double corrected_cps(double cps, double coeff, double load);


#endif // HIGHLOADCOEFFSCALCULATOR_H
