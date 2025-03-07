#pragma once

#include <array>

/* (c) 2022 Lucas V. Hartmann
 *
 * Least-squares first order derivative, for
 *   X[k] = D * k + k0, 
 * estimates D over N samples.
 *
 * Optimizations take place considering time is fixed and runs backwards:
 *   k=0 is current.
 *   k=1 is previous.
 *   k=n-1 is far past.
 * 
 * So:
 *   X[0]   = D*0     + k0
 *   X[1]   = D*1     + k0
 *   ...
 *   X[N-1] = D*(N-1) + k0
 * 
 * In matrix form:
 *  [ 0   1 ] [ D  ] = [ X[0]   ]
 *  [ 1   1 ] [ k0 ]   [ X[1]   ]
 *  [ 2   1 ]          [ X[2]   ]
 *  [ ...   ]          [  ...   ]
 *  [ N-1 1 ]          [ X[N-1] ]
 *  M          D     = X
 * 
 * So least squares is:
 *   D = inv(M' * M) * M' * X
 * 
 * M' * M is a 2x2 matrix:
 *   [ SS S ]
 *   [ S  N ]
 * where SS = sum(k*k) for k=0...n-1
 * and   S  = sum(k)   for k=0...n-1.
 * 
 * inv(M' * M) then becomes:
 * 1/det * [  N -S ]
 *         [ -S SS ]
 * where det = N*SS - S*S.
 * 
 * M' * X results in a 2x1 matrix:
 *   [ sum(k*x[k]) ]
 *   [ sum(  x[k]) ]
 * 
 * The derivative can be calculated from the first row of the result of the product:
 *   ( inv(M' * M) ) * ( M' * X )
 * as:
 *   D = A * sum(k*x[k]) + B * sum[k]
 * where A = N/det
 * and   B = -S/det
 * are both compile-time constants.
 * 
 * However since the time k is backward the signal for D will be negated.
 */
template <unsigned N>
class dxdk {
	std::array<double, N> X;
	double D;
	
	static constexpr double SS() {
		double r = 0;
		for (unsigned i=0; i<N; i++)
			r += i*i;
		return r; 
	}
	static constexpr double S() {
		double r = 0;
		for (unsigned i=0; i<N; i++)
			r += i;
		return r; 
	}
	static constexpr double det = N*SS() - S()*S();
	static constexpr double A = N / det;
	static constexpr double B = -S() / det;
	
public:
	dxdk() {
		for (auto &x : X) x = NAN;
	}
	
	double operator () (double x) {
		D = 0;
		for (size_t i=0; i<X.size(); i++) {
			D -= (A * i + B) * x;
			std::swap(X[i], x);
		}
		return D;
	}
	
	size_t size() const {
		return X.size();
	}
	
	operator double() const {
		return D;
	}
};
