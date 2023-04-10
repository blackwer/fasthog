#include <cstring>
#include <hog.hpp>

#include <cmath>
#include <cstdio>
#include <memory>

#include <afvec/vectorclass.h>
#include <afvec/vectormath_trig.h>

constexpr double eps2 = 1E-14;

void normalize_histogram(double *hist, int n_cells_x, int n_cells_y, int n_bins) {
    for (int i_cell = 0; i_cell < n_cells_x * n_cells_y; ++i_cell) {
        const int hist_offset = i_cell * n_bins;

        double norm_factor = 0.0;
        for (int i_bin = 0; i_bin < n_bins; ++i_bin)
            norm_factor += hist[hist_offset + i_bin] * hist[hist_offset + i_bin];
        if (!norm_factor)
            continue;

        norm_factor = 1.0 / sqrt(norm_factor + eps2);
        double norm_factor2 = 0.0;
        for (int i_bin = 0; i_bin < n_bins; ++i_bin) {
            hist[hist_offset + i_bin] *= norm_factor;
            hist[hist_offset + i_bin] = std::min(0.2, hist[hist_offset + i_bin]);
            norm_factor2 += hist[hist_offset + i_bin] * hist[hist_offset + i_bin];
        }

        norm_factor2 = 1.0 / sqrt(norm_factor2 + eps2);
        for (int i_bin = 0; i_bin < n_bins; ++i_bin)
            hist[hist_offset + i_bin] *= norm_factor2;
    }
}

void build_histogram(const double *magnitude, const double *orientation, int nrows, int ncols, int rows_per_cell,
                     int cols_per_cell, int n_bins, double *hist) {
    const int n_cells_y = nrows / rows_per_cell;
    const int n_cells_x = ncols / cols_per_cell;

    for (int y = 0; y < ncols; ++y) {
        const int y_cell = y / rows_per_cell;
        const int row_offset = y_cell * n_cells_x * n_bins;
        for (int x = 0; x < nrows; ++x) {
            const int x_cell = x / cols_per_cell;
            const int hist_offset = row_offset + x_cell * n_bins;

            const double angle = orientation[y * ncols + x];
            const double mag = magnitude[y * ncols + x];
            int high_bin = angle + 0.5;
            int low_bin = high_bin - 1;

            const double low_vote = mag * (high_bin + 0.5 - angle);
            const double high_vote = mag - low_vote;
            if (high_bin < 1)
                low_bin = n_bins - 1;
            if (high_bin >= n_bins)
                high_bin = 0;

            hist[hist_offset + low_bin] += low_vote;
            hist[hist_offset + high_bin] += high_vote;
        }
    }

    normalize_histogram(hist, n_cells_x, n_cells_y, n_bins);
}

void magnitude_orientation(const double *gx, const double *gy, int N, int n_bins, double *magnitude,
                           double *orientation) {
    const double scale_factor = 0.5 * n_bins / M_PI;
    const double shift = 2 * M_PI;
    for (int i = 0; i < N; i += 4) {
        Vec4d GX, GY, MAG, ORIENTATION;
        GX.load(gx + i);
        GY.load(gy + i);
        MAG = sqrt(GX * GX + GY * GY);
        ORIENTATION = atan2(GY, GX);
        ORIENTATION = scale_factor * if_add(ORIENTATION < 0, ORIENTATION, shift);
        MAG.store(magnitude + i);
        ORIENTATION.store(orientation + i);
    }
}

void gradient(const double *img, int nrows, int ncols, double *gx, double *gy) {
    for (int y = 0; y < nrows; ++y) {
        gx[y * ncols] = -img[y * ncols] + img[y * ncols + 1];
        const int yoff = y * ncols;
        for (int x = 1; x < ncols - 1; ++x)
            gx[yoff + x] = -img[yoff + x - 1] + img[yoff + x + 1];
        gx[(y + 1) * ncols - 1] = -img[(y + 1) * ncols - 2] + img[(y + 1) * ncols - 1];
    }

    for (int x = 0; x < ncols; ++x) {
        gy[x] = img[x] - img[ncols + x];
        for (int y = 1; y < nrows - 1; ++y)
            gy[y * ncols + x] = img[(y - 1) * ncols + x] - img[(y + 1) * ncols + x];
        gy[(nrows - 1) * ncols + x] = img[(nrows - 2) * ncols + x] - img[(nrows - 1) * ncols + x];
    }
}

extern "C" {
void hog(const double *img, int ncols, int nrows, int cell_size_x, int cell_size_y, int n_bins, double *hist) {
    const int N = nrows * ncols;
    const int n_cells_x = ncols / cell_size_x;
    const int n_cells_y = nrows / cell_size_x;
    double *mempool = new double[4 * N];
    double *gx = mempool + 0 * N;
    double *gy = mempool + 1 * N;
    double *magnitude = mempool + 2 * N;
    double *orientation = mempool + 3 * N;

    gradient(img, nrows, ncols, gx, gy);
    magnitude_orientation(gx, gy, N, n_bins, magnitude, orientation);
    memset(hist, 0, n_cells_x * n_cells_y * n_bins * sizeof(double));
    build_histogram(magnitude, orientation, nrows, ncols, cell_size_y, cell_size_x, n_bins, hist);
    delete[] mempool;
}
}
