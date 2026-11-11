#include "highload_coeffs_calculator.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include <Eigen/Dense>

namespace {

void solveEquation(const Eigen::MatrixXd& A, const Eigen::VectorXd& b, const Eigen::VectorXd& w,
                   bool uncertaintiesKnown, Eigen::VectorXd& x, Eigen::MatrixXd& covX)
{
    const Eigen::Index n = A.rows();
    const Eigen::Index p = A.cols();

    Eigen::MatrixXd Aw = A;
    Eigen::VectorXd bw = b;

    for (Eigen::Index i = 0; i < b.size(); ++i) {
        const double sqrtWeight = std::sqrt(w(i));
        Aw.row(i) *= sqrtWeight;
        bw(i)     *= sqrtWeight;
    }

    // get qr decomposition
    auto qr = Aw.colPivHouseholderQr();
    if (qr.rank() < p) {
        throw std::runtime_error("Columns A are linearly dependent");
    }

    // solve qr decomposition
    x = qr.solve(bw);

    // compute covariance matrix using qr decomposition
    // it is the same as covX = (Aw.transpose() * Aw).inverse();
    const Eigen::MatrixXd B =
        qr.solve(Eigen::MatrixXd::Identity(n, n));
    covX = B * B.transpose();

    // degree of freedom
    const Eigen::Index dof = n - p;

    // using scatter (residuals) to calculate covariance matrix
    if (dof > 0) {
        const Eigen::VectorXd weightedResidual = bw - Aw * x;
        // scale from residuals (scatter)
        const double residualScale =
            weightedResidual.squaredNorm() / static_cast<double>(dof);

        // correct on residuals only if residuals are greater than unc.
        const double covarianceScale =
            uncertaintiesKnown
                ? std::max(1.0, residualScale)
                : residualScale;

        covX *= covarianceScale;
    }
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

    // weights w_i = 1 / sigma_i^2
    // if all dB == 0, set them to 1
    const bool uncertaintiesKnown = !(dB.array() == 0.0).all();
    Eigen::VectorXd w(N);
    if (uncertaintiesKnown) {
        if (!dB.allFinite() || (dB.array() <= 0.0).any()) {
            throw std::invalid_argument("All measurement uncertainties must be positive");
        }
        w = 1.0 / dB.array().square();
    } else {
        w.setOnes();
    }

    // solve equation
    Eigen::VectorXd x;
    Eigen::MatrixXd covX;
    solveEquation(A, b, w, uncertaintiesKnown, x, covX);

    // Ca -- coefficient A
    const double s0 = x(0);  // count rate at zero load
    if (s0 <= 0.0) {
        throw std::runtime_error("cannot solve the equation, check input data");
    }
    const double Ca = x(1) / s0;  // x(1) = Ca * s0

    // dCa -- uncertainty of coefficient A
    Eigen::Vector2d gradient;
    gradient << -x(1) / (s0 * s0), 1.0 / s0;
    const double varCa = gradient.dot(covX * gradient);
    const double dCa = std::sqrt(std::max(0.0, varCa));

    const Eigen::VectorXd residual = b - A * x;
    // TODO: actually it is not chi2, find a correct name
    const double chi2 = (w.array() * residual.array().square()).sum() / w.sum();

    return {Ca, dCa, chi2, s0};
}


double corrected_cps(double cps, double coeff, double load) {
    // s_i = s_0 - s_0 C_a L_i
    return cps - cps * coeff * load;
}
