/*
    Copyright (C) 2010  EPFL (Ecole Polytechnique Fédérale de Lausanne)
    Laboratory CNBI (Chair in Non-Invasive Brain-Machine Interface)
    Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <xdfio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "copy_xdf.h"

#define NSAMPLE	23

int copy_xdf(const char* genfilename, const char* reffilename, int fformat)
{
	struct xdf *dst = NULL, *src = NULL;
	struct xdfch *dstch, *srcch;
	unsigned int ich = 0, samplesize, evttype;
	int nch, retcode = -1;
	void* buffer = NULL;
	ssize_t nssrc, nsdst;
	size_t nstot, stride[1];
	int i, nevent, nevtcode, evtcode;
	double onset, duration;
	const char* desc;

	src = xdf_open(reffilename, XDF_READ, fformat);
	if (!src) {
		fprintf(stderr, 
		       "\tFailed opening reference file: (%i) %s\n",
		       errno, strerror(errno));
		goto exit;
	}
		
	dst = xdf_open(genfilename, XDF_WRITE, fformat);
	if (!dst) {
		fprintf(stderr,
		       "\tFailed opening file for writing: (%i) %s\n",
		       errno, strerror(errno));
		goto exit;
	}

	// Copy header and configuration
	xdf_copy_conf(dst, src);
	while ((srcch = xdf_get_channel(src, ich))) {
		xdf_set_chconf(srcch, XDF_CF_ARRINDEX, 0, XDF_NOF);
		dstch = xdf_add_channel(dst, NULL);
		if (xdf_copy_chconf(dstch, srcch)) {
			fprintf(stderr, "\tFailed copying channel %i: (%i) %s\n",
			        ich, errno, strerror(errno));
			goto exit;
		}
		ich++;
	}
	xdf_get_conf(src, XDF_F_NCHANNEL, &nch,
	                  XDF_F_NEVENT, &nevent, XDF_F_NEVTTYPE, &nevtcode, XDF_NOF);
	if (nch != (int)ich) {
		fprintf(stderr, "\tich=%u, nch=%i\n", ich, nch);
		goto exit;
	}

	// Copy event code table
	for (i=0; i<nevtcode; i++) {
		if (xdf_get_evttype(src, i, &evtcode, &desc) < 0
		  || xdf_add_evttype(dst, evtcode, desc) < 0)
			goto exit;
	}

	samplesize = nch*4;
	buffer = malloc(samplesize*NSAMPLE);
	stride[0] = samplesize;
	xdf_define_arrays(src, 1, stride);
	xdf_define_arrays(dst, 1, stride);
	xdf_prepare_transfer(src);
	xdf_prepare_transfer(dst);

	if ( (xdf_seek(src, 1000, SEEK_CUR) < 0)
	    || (xdf_seek(src, 0, SEEK_SET) < 0) ) {
		fprintf(stderr, "\txdf_seek function failed: %s\n", strerror(errno));
		goto exit;
	}

	nstot = 0;
	while (1) {
		nssrc = xdf_read(src, NSAMPLE, buffer);
		if (nssrc < 0) {
			fprintf(stderr, 
			       "\tfailed reading a chunk of %i samples "
			       "after %u samples\nerror caught (%i), %s\n",
			       (int)NSAMPLE, (unsigned int)nstot,
			       errno, strerror(errno));
			goto exit;
		}
		if (nssrc == 0)
			break;
		nsdst = xdf_write(dst, nssrc, buffer);
		if (nsdst != nssrc) {
			fprintf(stderr, 
			       "\tfailed writing a chunk of %i samples "
			       "after %i samples\nerror caught (%i), %s\n",
			       (unsigned int)nssrc, (unsigned int)nstot, 
			       errno, strerror(errno));
			goto exit;
		}
		nstot += nsdst;
	}


	for (i=0; i<nevent; i++) {
		if (xdf_get_event(src, i, &evttype, &onset, &duration) < 0
		  || xdf_add_event(dst, evttype, onset, duration) < 0)
			goto exit;
	}
	retcode = 0;

exit:
	free(buffer);
	xdf_close(src);
	xdf_close(dst);

	return retcode;
}
