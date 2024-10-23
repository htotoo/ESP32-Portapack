
#include <math.h>
#include "brent.h"
#include "sgp4pred.h"

#define ITMAX 100  // Here ITMAX is the maximum allowed number of iterations;
#define R 0.61803399
#define C 0.3819660    // CGOLD is the golden ratio;
#define ZEPS 2.22e-15  // ZEPS is a small number that protects against trying to achieve fractional accuracy for a minimum that happens to be exactly zero.

#define SHFT2(a, b, c) \
    (a) = (b);         \
    (b) = (c);
#define SHFT3(a, b, c, d) \
    (a) = (b);            \
    (b) = (c);            \
    (c) = (d);

double brentmin(double ax, double bx, double cx, double (Sgp4::*f)(double), double tol, double* xmin, Sgp4* obj)
// Given a function f, and given a bracketing triplet of abscissas ax, bx, cx (such that bx is
// between ax and cx, and f(bx) is less than both f(ax) and f(cx)), this routine isolates
// the minimum to a fractional precision of about tol using Brent’s method. The abscissa of
// the minimum is returned as xmin, and the minimum function value is returned as brent, the
// returned function value.
{
    int iter;
    double a, b, d, etemp, fu, fv, fw, fx, p, q, r, tol1, tol2, u, v, w, x, xm;
    double e = 0.0;  // This will be the distance moved on the step before last.

    a = (ax < cx ? ax : cx);  // a and b must be in ascending order,
    b = (ax > cx ? ax : cx);  // but input abscissas need not be.
    x = w = v = bx;           // Initializations...
    fw = fv = fx = (obj->*f)(x);
    for (iter = 1; iter <= ITMAX; iter++) {  // Main program loop.
        xm = 0.5 * (a + b);
        tol2 = 2.0 * (tol1 = tol + ZEPS);              // 2.0*(tol1=tol*fabs(x)+ZEPS);
        if (fabs(x - xm) <= (tol2 - 0.5 * (b - a))) {  // Test for done here.
            *xmin = x;
            return fx;
        }
        if (fabs(e) > tol1) {  // Construct a trial parabolic fit.
            r = (x - w) * (fx - fv);
            q = (x - v) * (fx - fw);
            p = (x - v) * q - (x - w) * r;
            q = 2.0 * (q - r);
            if (q > 0.0) p = -p;
            q = fabs(q);
            etemp = e;
            e = d;
            if (fabs(p) >= fabs(0.5 * q * etemp) || p <= q * (a - x) || p >= q * (b - x))
                d = C * (e = (x >= xm ? a - x : b - x));
            // The above conditions determine the acceptability of the parabolic fit. Here we
            // take the golden section step into the larger of the two segments.
            else {
                d = p / q;  // Take the parabolic step.
                u = x + d;
                if (u - a < tol2 || b - u < tol2)
                    d = copysign(tol1, xm - x);
            }
        } else {
            d = C * (e = (x >= xm ? a - x : b - x));
        }
        u = (fabs(d) >= tol1 ? x + d : x + copysign(tol1, d));
        fu = (obj->*f)(u);
        // This is the one function evaluation per iteration.
        if (fu <= fx) {  // Now decide what to do with our func
            if (u >= x)
                a = x;
            else
                b = x;         // tion evaluation.
            SHFT3(v, w, x, u)  // Housekeeping follows:
            SHFT3(fv, fw, fx, fu)
        } else {
            if (u < x)
                a = u;
            else
                b = u;
            if (fu <= fw || w == x) {
                v = w;
                w = u;
                fv = fw;
                fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u;
                fv = fu;
            }
        }  // Done with housekeeping. Back for
    }  // another iteration.
    // nrerror("Too many iterations in brent");
    *xmin = -1.0;  // Never get here.
    return fx;
}

double zbrent(double (Sgp4::*func)(double), double x1, double x2, double tol, Sgp4* obj)
// Using Brent’s method, find the root of a function func known to lie between x1 and x2. The
// root, returned as zbrent, will be refined until its accuracy is tol.
{
    int iter;
    double a = x1, b = x2, c = x2, d, e = 0, min1, min2;
    double fa = (obj->*func)(a), fb = (obj->*func)(b), fc, p, q, r, s, tol1, xm;

    if ((fa > 0.0 && fb > 0.0) || (fa < 0.0 && fb < 0.0))
        return -1.0;
    fc = fb;
    for (iter = 1; iter <= ITMAX; iter++) {
        if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) {
            c = a;    // Rename a, b, c and adjust bounding interval
            fc = fa;  // d.
            e = d = b - a;
        }
        if (fabs(fc) < fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        tol1 = 2.0 * ZEPS + 0.5 * tol;  // Convergence check.
        xm = 0.5 * (c - b);
        if (fabs(xm) <= tol1 || fb == 0.0) return b;
        if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
            s = fb / fa;  // Attempt inverse quadratic interpolation.
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                q = fa / fc;
                r = fb / fc;
                p = s * (2.0 * xm * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q;  // Check whether in bounds.
            p = fabs(p);
            min1 = 3.0 * xm * q - fabs(tol1 * q);
            min2 = fabs(e * q);
            if (2.0 * p < (min1 < min2 ? min1 : min2)) {
                e = d;  // Accept interpolation.
                d = p / q;
            } else {
                d = xm;  // Interpolation failed, use bisection.
                e = d;
            }
        } else {  // Bounds decreasing too slowly, use bisection.
            d = xm;
            e = d;
        }
        a = b;  // Move last best guess to a.
        fa = fb;
        if (fabs(d) > tol1)  // Evaluate new trial root.
            b += d;
        else
            b += copysign(tol1, xm);
        fb = (obj->*func)(b);
    }
    // nrerror("Maximum number of iterations exceeded in zbrent");
    return -1.0;  // Never get here.
}
