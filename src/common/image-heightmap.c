/*
 * FFT based normalmap to heightmap converter
 * Copyright (C) 2010  Rudolf Polzer
 * Copyright (c) 2024 Quetoo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "quetoo.h"

#define C99

#ifdef C99
#include <complex.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <fftw3.h>

#define TWO_PI (4*atan2(1,1) * 2)

int floatcmp(const void *a_, const void *b_)
{
	float a = *(float *)a_;
	float b = *(float *)b_;
	if(a < b)
		return -1;
	if(a > b)
		return +1;
	return 0;
}

void nmap_to_hmap(unsigned char *map, const unsigned char *refmap, int w, int h, double scale, double offset, const double *filter, int filterw, int filterh, int renormalize, double highpass, int use_median)
{
	int x, y;
	int i, j;
	double fx, fy;
	double ffx, ffy;
	double nx, ny, nz;
	double v, vmin, vmed, vmax;
#ifndef C99
	double save;
#endif
	float *medianbuf = (float *) malloc(w*h * sizeof(*medianbuf));
	fftw_complex *imgspace1 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_complex *imgspace2 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_complex *freqspace1 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_complex *freqspace2 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_plan i12f1 = fftw_plan_dft_2d(h, w, imgspace1, freqspace1, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_plan i22f2 = fftw_plan_dft_2d(h, w, imgspace2, freqspace2, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_plan f12i1 = fftw_plan_dft_2d(h, w, freqspace1, imgspace1, FFTW_BACKWARD, FFTW_ESTIMATE);

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
		/*
		 * unnormalized normals:
		 * n_x = -dh/dx
		 * n_y = -dh/dy
		 * n_z = -dh/dh = -1
		 * BUT: darkplaces uses inverted normals, n_y actually is dh/dy by image pixel coordinates
		 */
		nx = ((int)map[(w*y+x)*4+2] - 127.5) / 128;
		ny = ((int)map[(w*y+x)*4+1] - 127.5) / 128;
		nz = ((int)map[(w*y+x)*4+0] - 127.5) / 128;

		/* reconstruct the derivatives from here */
#ifdef C99
		imgspace1[(w*y+x)] =  nx / nz * w; /* = dz/dx */
		imgspace2[(w*y+x)] = -ny / nz * h; /* = dz/dy */
#else
		imgspace1[(w*y+x)][0] =  nx / nz * w; /* = dz/dx */
		imgspace1[(w*y+x)][1] = 0;
		imgspace2[(w*y+x)][0] = -ny / nz * h; /* = dz/dy */
		imgspace2[(w*y+x)][1] = 0;
#endif

		if(renormalize)
		{
			double v = nx * nx + ny * ny + nz * nz;
			if(v > 0)
			{
				v = 1/sqrt(v);
				nx *= v;
				ny *= v;
				nz *= v;
				map[(w*y+x)*4+2] = floor(nx * 127.5 + 128);
				map[(w*y+x)*4+1] = floor(ny * 127.5 + 128);
				map[(w*y+x)*4+0] = floor(nz * 127.5 + 128);
			}
		}
	}

	/* see http://www.gamedev.net/community/forums/topic.asp?topic_id=561430 */

	fftw_execute(i12f1);
	fftw_execute(i22f2);

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
		fx = x * 1.0 / w;
		fy = y * 1.0 / h;
		if(fx > 0.5)
			fx -= 1;
		if(fy > 0.5)
			fy -= 1;
		if(filter)
		{
			/* discontinous case; we must invert whatever "filter" would do on (x, y)! */
#ifdef C99
			fftw_complex response_x = 0;
			fftw_complex response_y = 0;
			double sum;
			for(i = -filterh / 2; i <= filterh / 2; ++i)
				for(j = -filterw / 2; j <= filterw / 2; ++j)
				{
					response_x += filter[(i + filterh / 2) * filterw + j + filterw / 2] * cexp(-_Complex_I * TWO_PI * (j * fx + i * fy));
					response_y += filter[(i + filterh / 2) * filterw + j + filterw / 2] * cexp(-_Complex_I * TWO_PI * (i * fx + j * fy));
				}

			/*
			 * we know:
			 *   fourier(df/dx)_xy = fourier(f)_xy * response_x
			 *   fourier(df/dy)_xy = fourier(f)_xy * response_y
			 * mult by conjugate of response_x, response_y:
			 *   conj(response_x) * fourier(df/dx)_xy = fourier(f)_xy * |response_x^2|
			 *   conj(response_y) * fourier(df/dy)_xy = fourier(f)_xy * |response_y^2|
			 * and
			 *   fourier(f)_xy = (conj(response_x) * fourier(df/dx)_xy + conj(response_y) * fourier(df/dy)_xy) / (|response_x|^2 + |response_y|^2)
			 */

			sum = cabs(response_x) * cabs(response_x) + cabs(response_y) * cabs(response_y);

			if(sum > 0)
				freqspace1[(w*y+x)] = (conj(response_x) * freqspace1[(w*y+x)] + conj(response_y) * freqspace2[(w*y+x)]) / sum;
			else
				freqspace1[(w*y+x)] = 0;
#else
			fftw_complex response_x = {0, 0};
			fftw_complex response_y = {0, 0};
			double sum;
			for(i = -filterh / 2; i <= filterh / 2; ++i)
				for(j = -filterw / 2; j <= filterw / 2; ++j)
				{
					response_x[0] += filter[(i + filterh / 2) * filterw + j + filterw / 2] * cos(-TWO_PI * (j * fx + i * fy));
					response_x[1] += filter[(i + filterh / 2) * filterw + j + filterw / 2] * sin(-TWO_PI * (j * fx + i * fy));
					response_y[0] += filter[(i + filterh / 2) * filterw + j + filterw / 2] * cos(-TWO_PI * (i * fx + j * fy));
					response_y[1] += filter[(i + filterh / 2) * filterw + j + filterw / 2] * sin(-TWO_PI * (i * fx + j * fy));
				}

			sum = response_x[0] * response_x[0] + response_x[1] * response_x[1]
				+ response_y[0] * response_y[0] + response_y[1] * response_y[1];

			if(sum > 0)
			{
				double s = freqspace1[(w*y+x)][0];
				freqspace1[(w*y+x)][0] = (response_x[0] * s                      + response_x[1] * freqspace1[(w*y+x)][1] + response_y[0] * freqspace2[(w*y+x)][0] + response_y[1] * freqspace2[(w*y+x)][1]) / sum;
				freqspace1[(w*y+x)][1] = (response_x[0] * freqspace1[(w*y+x)][1] - response_x[1] * s                      + response_y[0] * freqspace2[(w*y+x)][1] - response_y[1] * freqspace2[(w*y+x)][0]) / sum;
			}
			else
			{
				freqspace1[(w*y+x)][0] = 0;
				freqspace1[(w*y+x)][1] = 0;
			}
#endif
		}
		else
		{
			/* continuous integration case */
			/* these must have the same sign as fx and fy (so ffx*fx + ffy*fy is nonzero), otherwise do not matter */
			/* it basically decides how artifacts are distributed */
			ffx = fx;
			ffy = fy;
#ifdef C99
			if(fx||fy)
				freqspace1[(w*y+x)] = _Complex_I * (ffx * freqspace1[(w*y+x)] + ffy * freqspace2[(w*y+x)]) / (ffx*fx + ffy*fy) / TWO_PI;
			else
				freqspace1[(w*y+x)] = 0;
#else
			if(fx||fy)
			{
				save = freqspace1[(w*y+x)][0];
				freqspace1[(w*y+x)][0] = -(ffx * freqspace1[(w*y+x)][1] + ffy * freqspace2[(w*y+x)][1]) / (ffx*fx + ffy*fy) / TWO_PI;
				freqspace1[(w*y+x)][1] =  (ffx * save + ffy * freqspace2[(w*y+x)][0]) / (ffx*fx + ffy*fy) / TWO_PI;
			}
			else
			{
				freqspace1[(w*y+x)][0] = 0;
				freqspace1[(w*y+x)][1] = 0;
			}
#endif
		}
		if(highpass > 0)
		{
			double f1 = (fabs(fx)*highpass);
			double f2 = (fabs(fy)*highpass);
			/* if either of them is < 1, phase out (min at 0.5) */
			double f =
				(f1 <= 0.5 ? 0 : (f1 >= 1 ? 1 : ((f1 - 0.5) * 2.0)))
				*
				(f2 <= 0.5 ? 0 : (f2 >= 1 ? 1 : ((f2 - 0.5) * 2.0)));
#ifdef C99
			freqspace1[(w*y+x)] *= f;
#else
			freqspace1[(w*y+x)][0] *= f;
			freqspace1[(w*y+x)][1] *= f;
#endif
		}
	}

	fftw_execute(f12i1);

	/* renormalize, find min/max */
	vmin = vmed = vmax = 0;
	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
#ifdef C99
		v = creal(imgspace1[(w*y+x)] /= pow(w*h, 1.5));
#else
		v = (imgspace1[(w*y+x)][0] /= pow(w*h, 1.5));
		/*
		 * imgspace1[(w*y+x)][1] /= pow(w*h, 1.5);
		 * this value is never used
		 */
#endif
		if(v < vmin || (x == 0 && y == 0))
			vmin = v;
		if(v > vmax || (x == 0 && y == 0))
			vmax = v;
		medianbuf[w*y+x] = v;
	}
	qsort(medianbuf, w*h, sizeof(*medianbuf), floatcmp);
	if(w*h % 2)
		vmed = medianbuf[(w*h-1)/2];
	else
		vmed = (medianbuf[(w*h)/2] + medianbuf[(w*h-2)/2]) * 0.5;

	if(refmap)
	{
		double f, a;
		double o, s;
		double sa, sfa, sffa, sfva, sva;
		double mi, ma;
		sa = sfa = sffa = sfva = sva = 0;
		mi = 1;
		ma = -1;
		for(y = 0; y < h; ++y)
		for(x = 0; x < w; ++x)
		{
			a = (int)refmap[(w*y+x)*4+3];
			v = (refmap[(w*y+x)*4+0]*0.114 + refmap[(w*y+x)*4+1]*0.587 + refmap[(w*y+x)*4+2]*0.299);
			v = (v - 128.0) / 127.0;
#ifdef C99
			f = creal(imgspace1[(w*y+x)]);
#else
			f = imgspace1[(w*y+x)][0];
#endif
			if(a <= 0)
				continue;
			if(v < mi)
				mi = v;
			if(v > ma)
				ma = v;
			sa += a;
			sfa += f*a;
			sffa += f*f*a;
			sfva += f*v*a;
			sva += v*a;
		}
		if(mi < ma)
		{
			/* linear regression ftw */
			o = (sfa*sfva - sffa*sva) / (sfa*sfa-sa*sffa);
			s = (sfa*sva - sa*sfva) / (sfa*sfa-sa*sffa);
		}
		else /* all values of v are equal, so we cannot get scale; we can still get offset */
		{
			o = ((sva - sfa) / sa);
			s = 1;
		}

		/*
		 * now apply user-given offset and scale to these values
		 * (x * s + o) * scale + offset
		 * x * s * scale + o * scale + offset
		 */
		offset += o * scale;
		scale *= s;
	}
	else if(scale == 0)
	{
		/*
		 * map vmin to -1
		 * map vmax to +1
		 */
		scale = 2 / (vmax - vmin);
		offset = -(vmax + vmin) / (vmax - vmin);
	}
	else if(use_median)
	{
		/*
		 * negative scale = match median to offset
		 * we actually want (v - vmed) * scale + offset
		 */
		offset -= vmed * scale;
	}

	printf("Min: %f\nAvg: %f\nMed: %f\nMax: %f\nScale: %f\nOffset: %f\nScaled-Min: %f\nScaled-Avg: %f\nScaled-Med: %f\nScaled-Max: %f\n",
		vmin, 0.0, vmed, vmax, scale, offset, vmin * scale + offset, offset, vmed * scale + offset, vmax * scale + offset);

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
#ifdef C99
		v = creal(imgspace1[(w*y+x)]);
#else
		v = imgspace1[(w*y+x)][0];
#endif
		v = v * scale + offset;
		if(v < -1)
			v = -1;
		if(v > 1)
			v = 1;
		map[(w*y+x)*4+3] = floor(128.5 + 127 * v); /* in heightmaps, we avoid pixel value 0 as many imaging apps cannot handle it */
	}

	fftw_destroy_plan(i12f1);
	fftw_destroy_plan(i22f2);
	fftw_destroy_plan(f12i1);

	fftw_free(freqspace2);
	fftw_free(freqspace1);
	fftw_free(imgspace2);
	fftw_free(imgspace1);
	free(medianbuf);
}

void hmap_to_nmap(unsigned char *map, int w, int h, int src_chan, double scale)
{
	int x, y;
	double fx, fy;
	double nx, ny, nz;
	double v;
#ifndef C99
	double save;
#endif

	fftw_complex *imgspace1 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_complex *imgspace2 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_complex *freqspace1 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_complex *freqspace2 = (fftw_complex *) fftw_malloc(w*h * sizeof(fftw_complex));
	fftw_plan i12f1 = fftw_plan_dft_2d(h, w, imgspace1, freqspace1, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_plan f12i1 = fftw_plan_dft_2d(h, w, freqspace1, imgspace1, FFTW_BACKWARD, FFTW_ESTIMATE);
	fftw_plan f22i2 = fftw_plan_dft_2d(h, w, freqspace2, imgspace2, FFTW_BACKWARD, FFTW_ESTIMATE);

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
		switch(src_chan)
		{
			case 0:
			case 1:
			case 2:
			case 3:
				v = map[(w*y+x)*4+src_chan];
				break;
			case 4:
				v = (map[(w*y+x)*4+0] + map[(w*y+x)*4+1] + map[(w*y+x)*4+2]) / 3;
				break;
			default:
			case 5:
				v = (map[(w*y+x)*4+0]*0.114 + map[(w*y+x)*4+1]*0.587 + map[(w*y+x)*4+2]*0.299);
				break;
		}
#ifdef C99
		imgspace1[(w*y+x)] = (v - 128.0) / 127.0;
#else
		imgspace1[(w*y+x)][0] = (v - 128.0) / 127.0;
		imgspace1[(w*y+x)][1] = 0;
#endif
		if(v < 1)
			v = 1; /* do not write alpha zero */
		map[(w*y+x)*4+3] = floor(v + 0.5);
	}

	/* see http://www.gamedev.net/community/forums/topic.asp?topic_id=561430 */

	fftw_execute(i12f1);

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
		fx = x;
		fy = y;
		if(fx > w/2)
			fx -= w;
		if(fy > h/2)
			fy -= h;
#ifdef DISCONTINUOUS
		fx = sin(fx * TWO_PI / w);
		fy = sin(fy * TWO_PI / h);
#else
#ifdef C99
		/* a lowpass to prevent the worst */
		freqspace1[(w*y+x)] *= 1 - pow(fabs(fx) / (double)(w/2), 1);
		freqspace1[(w*y+x)] *= 1 - pow(fabs(fy) / (double)(h/2), 1);
#else
		/* a lowpass to prevent the worst */
		freqspace1[(w*y+x)][0] *= 1 - pow(abs(fx) / (double)(w/2), 1);
		freqspace1[(w*y+x)][1] *= 1 - pow(abs(fx) / (double)(w/2), 1);
		freqspace1[(w*y+x)][0] *= 1 - pow(abs(fy) / (double)(h/2), 1);
		freqspace1[(w*y+x)][1] *= 1 - pow(abs(fy) / (double)(h/2), 1);
#endif
#endif
#ifdef C99
		freqspace2[(w*y+x)] = TWO_PI*_Complex_I * fy * freqspace1[(w*y+x)]; /* y derivative */
		freqspace1[(w*y+x)] = TWO_PI*_Complex_I * fx * freqspace1[(w*y+x)]; /* x derivative */
#else
		freqspace2[(w*y+x)][0] = -TWO_PI * fy * freqspace1[(w*y+x)][1]; /* y derivative */
		freqspace2[(w*y+x)][1] =  TWO_PI * fy * freqspace1[(w*y+x)][0];
		save = freqspace1[(w*y+x)][0];
		freqspace1[(w*y+x)][0] = -TWO_PI * fx * freqspace1[(w*y+x)][1]; /* x derivative */
		freqspace1[(w*y+x)][1] =  TWO_PI * fx * save;
#endif
	}

	fftw_execute(f12i1);
	fftw_execute(f22i2);

	scale /= (w*h);

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
#ifdef C99
		nx = creal(imgspace1[(w*y+x)]);
		ny = creal(imgspace2[(w*y+x)]);
#else
		nx = imgspace1[(w*y+x)][0];
		ny = imgspace2[(w*y+x)][0];
#endif
		nx /= w;
		ny /= h;
		nz = -1 / scale;
		v = -sqrt(nx*nx + ny*ny + nz*nz);
		nx /= v;
		ny /= v;
		nz /= v;
		ny = -ny; /* DP inverted normals */
		map[(w*y+x)*4+2] = floor(128 + 127.5 * nx);
		map[(w*y+x)*4+1] = floor(128 + 127.5 * ny);
		map[(w*y+x)*4+0] = floor(128 + 127.5 * nz);
	}

	fftw_destroy_plan(i12f1);
	fftw_destroy_plan(f12i1);
	fftw_destroy_plan(f22i2);

	fftw_free(freqspace2);
	fftw_free(freqspace1);
	fftw_free(imgspace2);
	fftw_free(imgspace1);
}

void hmap_to_nmap_local(unsigned char *map, int w, int h, int src_chan, double scale, const double *filter, int filterw, int filterh)
{
	int x, y;
	double nx, ny, nz;
	double v;
	int i, j;
	double *img_reduced = (double *) malloc(w*h * sizeof(double));

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
		switch(src_chan)
		{
			case 0:
			case 1:
			case 2:
			case 3:
				v = map[(w*y+x)*4+src_chan];
				break;
			case 4:
				v = (map[(w*y+x)*4+0] + map[(w*y+x)*4+1] + map[(w*y+x)*4+2]) / 3;
				break;
			default:
			case 5:
				v = (map[(w*y+x)*4+0]*0.114 + map[(w*y+x)*4+1]*0.587 + map[(w*y+x)*4+2]*0.299);
				break;
		}
		img_reduced[(w*y+x)] = (v - 128.0) / 127.0;
		if(v < 1)
			v = 1; /* do not write alpha zero */
		map[(w*y+x)*4+3] = floor(v + 0.5);
	}

	for(y = 0; y < h; ++y)
	for(x = 0; x < w; ++x)
	{
		nz = -1 / scale;
		nx = ny = 0;

		for(i = -filterh / 2; i <= filterh / 2; ++i)
			for(j = -filterw / 2; j <= filterw / 2; ++j)
			{
				nx += img_reduced[w*((y+i+h)%h)+(x+j+w)%w] * filter[(i + filterh / 2) * filterw + j + filterw / 2];
				ny += img_reduced[w*((y+j+h)%h)+(x+i+w)%w] * filter[(i + filterh / 2) * filterw + j + filterw / 2];
			}

		v = -sqrt(nx*nx + ny*ny + nz*nz);
		nx /= v;
		ny /= v;
		nz /= v;
		ny = -ny; /* DP inverted normals */
		map[(w*y+x)*4+2] = floor(128 + 127.5 * nx);
		map[(w*y+x)*4+1] = floor(128 + 127.5 * ny);
		map[(w*y+x)*4+0] = floor(128 + 127.5 * nz);
	}

	free(img_reduced);
}

/* START stuff that originates from image.c in DarkPlaces */
int image_width, image_height;

static const double filter_scharr3[3][3] = {
	{  -3/32.0, 0,  3/32.0 },
	{ -10/32.0, 0, 10/32.0 },
	{  -3/32.0, 0,  3/32.0 }
};

static const double filter_prewitt3[3][3] = {
	{ -1/6.0, 0, 1/6.0 },
	{ -1/6.0, 0, 1/6.0 },
	{ -1/6.0, 0, 1/6.0 }
};

/* pathologic for inverting */
static const double filter_sobel3[3][3] = {
	{ -1/8.0, 0, 1/8.0 },
	{ -2/8.0, 0, 2/8.0 },
	{ -1/8.0, 0, 1/8.0 }
};

/* pathologic for inverting */
static const double filter_sobel5[5][5] = {
	{ -1/128.0,  -2/128.0, 0,  2/128.0, 1/128.0 },
	{ -4/128.0,  -8/128.0, 0,  8/128.0, 4/128.0 },
	{ -6/128.0, -12/128.0, 0, 12/128.0, 6/128.0 },
	{ -4/128.0,  -8/128.0, 0,  8/128.0, 4/128.0 },
	{ -1/128.0,  -2/128.0, 0,  2/128.0, 1/128.0 }
};

/* pathologic for inverting */
static const double filter_prewitt5[5][5] = {
	{ -1/40.0, -2/40.0, 0, 2/40.0, 1/40.0 },
	{ -1/40.0, -2/40.0, 0, 2/40.0, 1/40.0 },
	{ -1/40.0, -2/40.0, 0, 2/40.0, 1/40.0 },
	{ -1/40.0, -2/40.0, 0, 2/40.0, 1/40.0 },
	{ -1/40.0, -2/40.0, 0, 2/40.0, 1/40.0 }
};

static const double filter_trivial[1][3] = {
	{ -0.5, 0, 0.5 }
};

