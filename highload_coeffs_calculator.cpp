#include "highload_coeffs_calculator.h"
#include <Eigen/Dense>

#include <stdexcept>
#include <iostream>
#include <cmath>


namespace {

void solveEquation(const Eigen::MatrixXd& A, const Eigen::VectorXd& b, const Eigen::VectorXd& w,
                   Eigen::VectorXd& x, Eigen::VectorXd& dx) {
    Eigen::MatrixXd Aw = A;
    Eigen::VectorXd bw = b;

    for (Eigen::Index i = 0; i < b.size(); ++i) {
        const double sqrtWeight = std::sqrt(w(i));
        Aw.row(i) *= sqrtWeight;
        bw(i)     *= sqrtWeight;
    }

    auto qr = Aw.colPivHouseholderQr();
    if (qr.rank() < 2) {
        throw std::runtime_error("Columns A are linearly dependent");
    }

    x = qr.solve(bw);
    Eigen::Matrix2d covX = (Aw.transpose() * Aw).inverse();
    dx = covX.diagonal().array().sqrt().matrix();
}

}


CalculationResults calculateHighLoadCoeffs(const Matrix& matrix) {
    /*
    Analytical formula
    Li -- load, matrix[:, 0]
    si -- peak_cps, matrix[:, 1]
    dsi -- dpeak_cps, matrix[:, 2]
    A = [[1, -L_i]]
    b = [s_i]
    dB = [dsi_i]
    x = [s0, Ca*s0]
    */

    size_t N = matrix.size();
    if (N < 2) {
        throw std::runtime_error("Matrix size is too small, need more or equal than 2 rows");
    }
    if (matrix[0].size() != 3) {
        throw std::runtime_error("Matrix size is unexpected, need 3 columns");
    }

    // fill matricesr
    Eigen::VectorXd L(N);
    Eigen::VectorXd b(N);
    Eigen::VectorXd dB(N);
    for (size_t i = 0; i < N; ++i) {
        L(i) = matrix[i][0];
        b(i) = matrix[i][1];
        dB(i) = matrix[i][2];
    }
    Eigen::MatrixXd A(N, 2);
    A.col(0) = Eigen::VectorXd::Ones(N);
    A.col(1) = -L;

    // if all dB == 0, set them 1
    bool has_unc = true;
    if ((dB.array() == 0.0).all()) {
        dB.fill(1.0);
        if (N == 2) has_unc = false;
    }

    // checks
    if ((dB.array() <= 0.0).any())
        throw std::invalid_argument("All unc. values must be positive");

    // weights w_i = 1 / sigma_i^2
    const Eigen::ArrayXd w = 1.0 / dB.array().square();

    // solve equation
    Eigen::VectorXd x;
    Eigen::VectorXd dx;
    solveEquation(A, b, w, x, dx);

    double s0 = x(0);  // count rate at zero load
    if (s0 <= 0.0) {
        throw std::runtime_error("cannot solve the equation, check input data");
    }
    double Ca = x(1) / s0;  // x(1) = Ca * s0
    double dCa = dx(1) / s0;
    double chi2 = (b - A * x).array().square().sum() / w.sum();

    if (!has_unc) {
        dCa = 0.0;
    }

    return {Ca, dCa, chi2, s0};
}


double corrected_cps(double cps, double coeff, double load) {
    // s_i = s_0 - s_0 C_a L_i
    return cps - cps * coeff * load;
}
