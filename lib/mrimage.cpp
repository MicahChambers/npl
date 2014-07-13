/*******************************************************************************
This file is part of Neuro Programs and Libraries (NPL), 

Written and Copyrighted by by Micah C. Chambers (micahc.vt@gmail.com)

The Neuro Programs and Libraries is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your option)
any later version.

The Neural Programs and Libraries are distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
the Neural Programs Library.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#include "mrimage.h"
#include "mrimage.txx"

#include "ndarray.h"
#include "nifti.h"
#include "byteswap.h"
#include "slicer.h"

#include "zlib.h"

#include <cstring>

namespace npl {

/* Functions */
int readNifti2Header(gzFile file, nifti2_header* header, bool* doswap, bool verbose);
int readNifti1Header(gzFile file, nifti1_header* header, bool* doswap, bool verbose);
int writeNifti1Image(MRImage* out, gzFile file);
int writeNifti2Image(MRImage* out, gzFile file);

template class MRImageStore<1, double>;
template class MRImageStore<1, float>;
template class MRImageStore<1, int64_t>;
template class MRImageStore<1, uint64_t>;
template class MRImageStore<1, int32_t>;
template class MRImageStore<1, uint32_t>;
template class MRImageStore<1, int16_t>;
template class MRImageStore<1, uint16_t>;
template class MRImageStore<1, int8_t>;
template class MRImageStore<1, uint8_t>;

template class MRImageStore<2, double>;
template class MRImageStore<2, float>;
template class MRImageStore<2, int64_t>;
template class MRImageStore<2, uint64_t>;
template class MRImageStore<2, int32_t>;
template class MRImageStore<2, uint32_t>;
template class MRImageStore<2, int16_t>;
template class MRImageStore<2, uint16_t>;
template class MRImageStore<2, int8_t>;
template class MRImageStore<2, uint8_t>;

template class MRImageStore<3, double>;
template class MRImageStore<3, float>;
template class MRImageStore<3, int64_t>;
template class MRImageStore<3, uint64_t>;
template class MRImageStore<3, int32_t>;
template class MRImageStore<3, uint32_t>;
template class MRImageStore<3, int16_t>;
template class MRImageStore<3, uint16_t>;
template class MRImageStore<3, int8_t>;
template class MRImageStore<3, uint8_t>;

template class MRImageStore<4, double>;
template class MRImageStore<4, float>;
template class MRImageStore<4, int64_t>;
template class MRImageStore<4, uint64_t>;
template class MRImageStore<4, int32_t>;
template class MRImageStore<4, uint32_t>;
template class MRImageStore<4, int16_t>;
template class MRImageStore<4, uint16_t>;
template class MRImageStore<4, int8_t>;
template class MRImageStore<4, uint8_t>;

template class MRImageStore<5, double>;
template class MRImageStore<5, float>;
template class MRImageStore<5, int64_t>;
template class MRImageStore<5, uint64_t>;
template class MRImageStore<5, int32_t>;
template class MRImageStore<5, uint32_t>;
template class MRImageStore<5, int16_t>;
template class MRImageStore<5, uint16_t>;
template class MRImageStore<5, int8_t>;
template class MRImageStore<5, uint8_t>;

template class MRImageStore<6, double>;
template class MRImageStore<6, float>;
template class MRImageStore<6, int64_t>;
template class MRImageStore<6, uint64_t>;
template class MRImageStore<6, int32_t>;
template class MRImageStore<6, uint32_t>;
template class MRImageStore<6, int16_t>;
template class MRImageStore<6, uint16_t>;
template class MRImageStore<6, int8_t>;
template class MRImageStore<6, uint8_t>;

template class MRImageStore<7, double>;
template class MRImageStore<7, float>;
template class MRImageStore<7, int64_t>;
template class MRImageStore<7, uint64_t>;
template class MRImageStore<7, int32_t>;
template class MRImageStore<7, uint32_t>;
template class MRImageStore<7, int16_t>;
template class MRImageStore<7, uint16_t>;
template class MRImageStore<7, int8_t>;
template class MRImageStore<7, uint8_t>;

template class MRImageStore<8, double>;
template class MRImageStore<8, float>;
template class MRImageStore<8, int64_t>;
template class MRImageStore<8, uint64_t>;
template class MRImageStore<8, int32_t>;
template class MRImageStore<8, uint32_t>;
template class MRImageStore<8, int16_t>;
template class MRImageStore<8, uint16_t>;
template class MRImageStore<8, int8_t>;
template class MRImageStore<8, uint8_t>;


/* Pre-Compile Certain Image Types */
MRImage* readMRImage(std::string filename, bool verbose)
{
	const size_t BSIZE = 1024*1024; //1M
	auto gz = gzopen(filename.c_str(), "rb");

	if(!gz) {
		cerr << "Error, could not open MRI Image " << filename << endl;
		return NULL;
	}
	gzbuffer(gz, BSIZE);
	
	MRImage* out = NULL;

	if((out = readNiftiImage(gz, verbose))) {
		gzclose(gz);
		return out;
	}

	return NULL;
}

int writeMRImage(MRImage* img, std::string fn, bool nifti2)
{
	if(!img)
		return -1;
	double version = 1;
	if(nifti2)
		version = 2;
	return img->write(fn, version);
}

template <typename T>
MRImage* readPixels(gzFile file, size_t vox_offset, 
		const std::vector<size_t>& dim, size_t pixsize, bool doswap)
{
	// jump to voxel offset
	gzseek(file, vox_offset, SEEK_SET);

	/* 
	 * Create Slicer Object to iterate through image slices
	 */

	// dim 0 is the fastest in nifti images, so go in that order
	std::list<size_t> order;
	for(size_t ii=0; ii<dim.size(); ii++) 
		order.push_back(ii);
	
	Slicer slicer(dim, order);

	T tmp(0);
	MRImage* out = NULL;

	// someday this all might be simplify by using MRImage* and the 
	// dbl or int64 functions, as long as we trust that the type is
	// going to be good enough to caputre the underlying pixle type
	switch(dim.size()) {
		case 1: {
			auto typed = new MRImageStore<1, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 2:{
			auto typed = new MRImageStore<2, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 3:{
			auto typed = new MRImageStore<3, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 4:{
			auto typed = new MRImageStore<4, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 5:{
			auto typed = new MRImageStore<5, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 6:{
			auto typed = new MRImageStore<6, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 7:{
			auto typed = new MRImageStore<7, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
		case 8:{
			auto typed = new MRImageStore<8, T>(dim);
			for(slicer.goBegin(); slicer.isEnd(); ++slicer) {
				gzread(file, &tmp, pixsize);
				if(doswap) swap<T>(&tmp);
				(*typed)[*slicer] = tmp;
			}
			out = typed;
			} break;
	};

	return out;
}

int readNifti1Header(gzFile file, nifti1_header* header, bool* doswap, 
		bool verbose)
{
	// seek to 0
	gzseek(file, 0, SEEK_SET);

	static_assert(sizeof(nifti1_header) == 348, "Error, nifti header packing failed");

	// read header
	gzread(file, header, sizeof(nifti1_header));
	std::cerr << header->magic << std::endl;
	if(strncmp(header->magic, "n+1", 3)) {
		gzclearerr(file);
		gzrewind(file);
		return 1;
	}

	// byte swap
	int64_t npixel = 1;
	std::cerr << header->sizeof_hdr << endl;
	if(header->sizeof_hdr != 348) {
		*doswap = true;
		swap(&header->sizeof_hdr);
		if(header->sizeof_hdr != 348) {
			std::cerr << "Swapped Header Size: " << header->sizeof_hdr << std::endl;
			swap(&header->sizeof_hdr);
			std::cerr << "UnSwapped Header Size: " << header->sizeof_hdr << std::endl;
			std::cerr << "Malformed nifti input" << std::endl;
			return -1;
		}
		swap(&header->ndim);
		for(size_t ii=0; ii<7; ii++)
			swap(&header->dim[ii]);
		swap(&header->intent_p1);
		swap(&header->intent_p2);
		swap(&header->intent_p3);
		swap(&header->intent_code);
		swap(&header->datatype);
		swap(&header->bitpix);
		swap(&header->slice_start);
		swap(&header->qfac);
		for(size_t ii=0; ii<7; ii++)
			swap(&header->pixdim[ii]);
		swap(&header->vox_offset);
		swap(&header->scl_slope);
		swap(&header->scl_inter);
		swap(&header->slice_end);
		swap(&header->cal_max);
		swap(&header->cal_min);
		swap(&header->slice_duration);
		swap(&header->toffset);
		swap(&header->glmax);
		swap(&header->glmin);
		swap(&header->qform_code);
		swap(&header->sform_code);
		
		for(size_t ii=0; ii<3; ii++)
			swap(&header->quatern[ii]);
		for(size_t ii=0; ii<3; ii++)
			swap(&header->qoffset[ii]);
		for(size_t ii=0; ii<12; ii++)
			swap(&header->saffine[ii]);

		for(int32_t ii=0; ii<header->ndim; ii++)
			npixel *= header->dim[ii];
	}
	
	if(verbose) {
		std::cerr << "sizeof_hdr=" << header->sizeof_hdr << std::endl;
		std::cerr << "data_type=" << header->data_type << std::endl;
		std::cerr << "db_name=" << header->db_name << std::endl;
		std::cerr << "extents=" << header->extents  << std::endl;
		std::cerr << "session_error=" << header->session_error << std::endl;
		std::cerr << "regular=" << header->regular << std::endl;

		std::cerr << "magic =" << header->magic  << std::endl;
		std::cerr << "datatype=" << header->datatype << std::endl;
		std::cerr << "bitpix=" << header->bitpix << std::endl;
		std::cerr << "ndim=" << header->ndim << std::endl;
		for(size_t ii=0; ii < 7; ii++)
			std::cerr << "dim["<<ii<<"]=" << header->dim[ii] << std::endl;
		std::cerr << "intent_p1 =" << header->intent_p1  << std::endl;
		std::cerr << "intent_p2 =" << header->intent_p2  << std::endl;
		std::cerr << "intent_p3 =" << header->intent_p3  << std::endl;
		std::cerr << "qfac=" << header->qfac << std::endl;
		for(size_t ii=0; ii < 7; ii++)
			std::cerr << "pixdim["<<ii<<"]=" << header->pixdim[ii] << std::endl;
		std::cerr << "vox_offset=" << header->vox_offset << std::endl;
		std::cerr << "scl_slope =" << header->scl_slope  << std::endl;
		std::cerr << "scl_inter =" << header->scl_inter  << std::endl;
		std::cerr << "cal_max=" << header->cal_max << std::endl;
		std::cerr << "cal_min=" << header->cal_min << std::endl;
		std::cerr << "slice_duration=" << header->slice_duration << std::endl;
		std::cerr << "toffset=" << header->toffset << std::endl;
		std::cerr << "glmax=" << header->glmax  << std::endl;
		std::cerr << "glmin=" << header->glmin  << std::endl;
		std::cerr << "slice_start=" << header->slice_start << std::endl;
		std::cerr << "slice_end=" << header->slice_end << std::endl;
		std::cerr << "descrip=" << header->descrip << std::endl;
		std::cerr << "aux_file=" << header->aux_file << std::endl;
		std::cerr << "qform_code =" << header->qform_code  << std::endl;
		std::cerr << "sform_code =" << header->sform_code  << std::endl;
		for(size_t ii=0; ii < 3; ii++){
			std::cerr << "quatern["<<ii<<"]=" 
				<< header->quatern[ii] << std::endl;
		}
		for(size_t ii=0; ii < 3; ii++){
			std::cerr << "qoffset["<<ii<<"]=" 
				<< header->qoffset[ii] << std::endl;
		}
		for(size_t ii=0; ii < 3; ii++) {
			for(size_t jj=0; jj < 4; jj++) {
				std::cerr << "saffine["<<ii<<"*4+"<<jj<<"]=" 
					<< header->saffine[ii*4+jj] << std::endl;
			}
		}
		std::cerr << "slice_code=" << header->slice_code << std::endl;
		std::cerr << "xyzt_units=" << header->xyzt_units << std::endl;
		std::cerr << "intent_code =" << header->intent_code  << std::endl;
		std::cerr << "intent_name=" << header->intent_name << std::endl;
		std::cerr << "dim_info.bits.freqdim=" << header->dim_info.bits.freqdim << std::endl;
		std::cerr << "dim_info.bits.phasedim=" << header->dim_info.bits.phasedim << std::endl;
		std::cerr << "dim_info.bits.slicedim=" << header->dim_info.bits.slicedim << std::endl;
	}
	
	return 0;
}

/* 
 * Nifti Readers 
 */
MRImage* readNiftiImage(gzFile file, bool verbose)
{
	bool doswap = false;
	int16_t datatype = 0;
	size_t start;
	std::vector<size_t> dim;
	size_t psize;
	int qform_code = 0;
	std::vector<double> pixdim;
	std::vector<double> offset;
	std::vector<double> quatern(3,0);
	double qfac;
	double slice_duration = 0;
	int slice_code = 0;
	int slice_start = 0;
	int slice_end = 0;
	int freqdim = 0;
	int phasedim = 0;
	int slicedim = 0;

	int ret = 0;
	nifti1_header header1;
	nifti2_header header2;
	if((ret = readNifti1Header(file, &header1, &doswap, verbose)) == 0) {
		start = header1.vox_offset;
		dim.resize(header1.ndim, 0);
		for(int64_t ii=0; ii<header1.ndim && ii < 7; ii++) {
			dim[ii] = header1.dim[ii];
			cerr << dim[ii] << endl;
		}
		psize = (header1.bitpix >> 3);
		qform_code = header1.qform_code;
		datatype = header1.datatype;

		slice_code = header1.slice_code;
		slice_duration = header1.slice_duration;
		slice_start = header1.slice_start;
		slice_end = header1.slice_end;
		freqdim = (int)(header1.dim_info.bits.freqdim)-1;
		phasedim = (int)(header1.dim_info.bits.phasedim)-1;
		slicedim = (int)(header1.dim_info.bits.slicedim)-1;

		// pixdim
		pixdim.resize(header1.ndim, 0);
		for(int64_t ii=0; ii<header1.ndim && ii < 7; ii++)
			pixdim[ii] = header1.pixdim[ii];

		// offset
		offset.resize(4, 0);
		for(int64_t ii=0; ii<header1.ndim && ii < 3; ii++)
			offset[ii] = header1.qoffset[ii];
		if(header1.ndim > 3)
			offset[3] = header1.toffset;

		// quaternion
		for(int64_t ii=0; ii<3 && ii<header1.ndim; ii++)
			quatern[ii] = header1.quatern[ii];
		qfac = header1.qfac;
	}

	if(ret!=0 && (ret = readNifti2Header(file, &header2, &doswap, verbose)) == 0) {
		start = header2.vox_offset;
		dim.resize(header2.ndim, 0);
		for(int64_t ii=0; ii<header2.ndim && ii < 7; ii++) {
			dim[ii] = header2.dim[ii];
			cerr << dim[ii] << endl;
		}
		psize = (header2.bitpix >> 3);
		qform_code = header2.qform_code;
		datatype = header2.datatype;
		
		slice_code = header2.slice_code;
		slice_duration = header2.slice_duration;
		slice_start = header2.slice_start;
		slice_end = header2.slice_end;
		freqdim = (int)(header2.dim_info.bits.freqdim)-1;
		phasedim = (int)(header2.dim_info.bits.phasedim)-1;
		slicedim = (int)(header2.dim_info.bits.slicedim)-1;

		// pixdim
		pixdim.resize(header2.ndim, 0);
		for(int64_t ii=0; ii<header2.ndim && ii < 7; ii++)
			pixdim[ii] = header2.pixdim[ii];
		
		// offset
		offset.resize(4, 0);
		for(int64_t ii=0; ii<header2.ndim && ii < 3; ii++)
			offset[ii] = header2.qoffset[ii];
		if(header2.ndim > 3)
			offset[3] = header2.toffset;
		
		// quaternion
		for(int64_t ii=0; ii<3 && ii<header2.ndim; ii++)
			quatern[ii] = header2.quatern[ii];
		qfac = header2.qfac;
	}

	MRImage* out;

	// create image
	switch(datatype) {
		// 8 bit
		case NIFTI_TYPE_INT8:
			out = readPixels<int8_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_UINT8:
			out = readPixels<uint8_t>(file, start, dim, psize, doswap);
		break;
		// 16  bit
		case NIFTI_TYPE_INT16:
			out = readPixels<int16_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_UINT16:
			out = readPixels<uint16_t>(file, start, dim, psize, doswap);
		break;
		// 32 bit
		case NIFTI_TYPE_INT32:
			out = readPixels<int32_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_UINT32:
			out = readPixels<uint32_t>(file, start, dim, psize, doswap);
		break;
		// 64 bit int 
		case NIFTI_TYPE_INT64:
			out = readPixels<int64_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_UINT64:
			out = readPixels<uint64_t>(file, start, dim, psize, doswap);
		break;
		// floats
		case NIFTI_TYPE_FLOAT32:
			out = readPixels<float>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_FLOAT64:
			out = readPixels<double>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_FLOAT128:
			out = readPixels<long double>(file, start, dim, psize, doswap);
		break;
		// RGB
		case NIFTI_TYPE_RGB24:
			out = readPixels<rgb_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_RGBA32:
			out = readPixels<rgba_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_COMPLEX256:
			out = readPixels<cquad_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_COMPLEX128:
			out = readPixels<cdouble_t>(file, start, dim, psize, doswap);
		break;
		case NIFTI_TYPE_COMPLEX64:
			out = readPixels<cfloat_t>(file, start, dim, psize, doswap);
		break;
	}

	if(!out)
		return NULL;

	/* 
	 * Now that we have an Image*, we can fill in the remaining values from 
	 * the header
	 */

	// figure out orientation
	if(qform_code > 0) {
		/*
		 * set spacing 
		 */
		for(size_t ii=0; ii<out->ndim(); ii++)
			out->space()[ii] = pixdim[ii];
		
		/* 
		 * set origin 
		 */
		// x,y,z
		for(size_t ii=0; ii<out->ndim(); ii++) {
			out->origin()[ii] = offset[ii];
		}
		cerr << "Origin: " << endl;
		for(size_t ii=0; ii<out->ndim(); ii++)
			cerr << out->origin()[ii] << endl;
		
		/* Copy Quaternions, and Make Rotation Matrix */

//		// copy quaternions, (we'll set a from R)
//		out->use_quaterns = true;
//		for(size_t ii=0; ii<3 && ii<out->ndim(); ii++)
//			out->quaterns[ii+1] = quatern[ii];
		
		// calculate a, copy others
		double b = quatern[0];
		double c = quatern[1];
		double d = quatern[2];
		double a = sqrt(1.0-(b*b+c*c+d*d));
		std::cerr << "Qa=" << a << std::endl;
		std::cerr << "Qb=" << b << std::endl;
		std::cerr << "Qc=" << c << std::endl;
		std::cerr << "Qd=" << d << std::endl;

		// calculate R, (was already identity)
		out->direction()(0, 0) = a*a+b*b-c*c-d*d;
		std::cerr << "00" << out->direction()(0,0) << std::endl;

		if(out->ndim() > 1) {
			out->direction()(0,1) = 2*b*c-2*a*d;
		std::cerr << "01" << out->direction()(0,1) << std::endl;
			out->direction()(1,0) = 2*b*c+2*a*d;
		std::cerr << "10" << out->direction()(1,0) << std::endl;
			out->direction()(1,1) = a*a+c*c-b*b-d*d;
		std::cerr << "11" << out->direction()(1,1) << std::endl;
		}

		if(qfac != -1)
			qfac = 1;
		
		if(out->ndim() > 2) {
			out->direction()(0,2) = qfac*(2*b*d+2*a*c);
		std::cerr << "02" << out->direction()(0,2) << std::endl;
			out->direction()(1,2) = qfac*(2*c*d-2*a*b);
		std::cerr << "12" << out->direction()(1,2) << std::endl;
			out->direction()(2,2) = qfac*(a*a+d*d-c*c-b*b);
		std::cerr << "22" << out->direction()(2,2) << std::endl;
			out->direction()(2,1) = 2*c*d+2*a*b;
		std::cerr << "21" << out->direction()(2,1) << std::endl;
			out->direction()(2,0) = 2*b*d-2*a*c;
		std::cerr << "20" << out->direction()(2,0) << std::endl;
		}

		// finally update affine, but scale pixdim[z] by qfac temporarily
		out->updateAffine();
//	} else if(header.sform_code > 0) {
//		/* use the sform, since no qform exists */
//
//		// origin, last column
//		double di = 0, dj = 0, dk = 0;
//		for(size_t ii=0; ii<3 && ii<out->ndim(); ii++) {
//			di += pow(header.saffine[4*ii+0],2); //column 0
//			dj += pow(header.saffine[4*jj+1],2); //column 1
//			dk += pow(header.saffine[4*kk+2],2); //column 2
//			out->origin()[ii] = header.saffine[4*ii+3]; //column 3
//		}
//		
//		// set direction and spacing
//		out->m_spacing[0] = sqrt(di);
//		out->m_dir[0*out->ndim()+0] = header.saffine[4*0+0]/di;
//
//		if(out->ndim() > 1) {
//			out->m_spacing[1] = sqrt(dj);
//			out->m_dir[0*out->ndim()+1] = header.saffine[4*0+1]/dj;
//			out->m_dir[1*out->ndim()+1] = header.saffine[4*1+1]/dj;
//			out->m_dir[1*out->ndim()+0] = header.saffine[4*1+0]/di;
//		}
//		if(out->ndim() > 2) {
//			out->m_spacing[2] = sqrt(dk);
//			out->m_dir[0*out->ndim()+2] = header.saffine[4*0+2]/dk;
//			out->m_dir[1*out->ndim()+2] = header.saffine[4*1+2]/dk;
//			out->m_dir[2*out->ndim()+2] = header.saffine[4*2+2]/dk;
//			out->m_dir[2*out->ndim()+1] = header.saffine[4*2+1]/dj;
//			out->m_dir[2*out->ndim()+0] = header.saffine[4*2+0]/di;
//		}
//
//		// affine matrix
//		updateAffine();
	} else {
		// only spacing changes
		for(size_t ii=0; ii<dim.size(); ii++)
			out->space()[ii] = pixdim[ii];
		out->updateAffine();
	}

	/************************************************************************** 
	 * Medical Imaging Varaibles Variables 
	 **************************************************************************/
	
	// direct copies
	out->m_slice_duration = slice_duration;
	out->m_slice_start = slice_start;
	out->m_slice_end = slice_end;
	out->m_freqdim = freqdim;
	out->m_phasedim = phasedim;
	out->m_slicedim = slicedim;

	if(out->m_slicedim > 0) {
		out->m_slice_timing.resize(out->dim(out->m_slicedim), NAN);
	}
			
	// we use the same encoding as nifti

	// slice timing
	switch(slice_code) {
		case NIFTI_SLICE_SEQ_INC:
			out->m_slice_order = SEQ;
			for(int ii=out->m_slice_start; ii<=out->m_slice_end; ii++)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
		break;
		case NIFTI_SLICE_SEQ_DEC:
			out->m_slice_order = RSEQ;
			for(int ii=out->m_slice_end; ii>=out->m_slice_start; ii--)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
		break;
		case NIFTI_SLICE_ALT_INC:
			out->m_slice_order = ALT;
			for(int ii=out->m_slice_start; ii<=out->m_slice_end; ii+=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
			for(int ii=out->m_slice_start+1; ii<=out->m_slice_end; ii+=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
		break;
		case NIFTI_SLICE_ALT_DEC:
			out->m_slice_order = RALT;
			for(int ii=out->m_slice_end; ii>=out->m_slice_start; ii-=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
			for(int ii=out->m_slice_end-1; ii>=out->m_slice_start; ii-=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
		break;
		case NIFTI_SLICE_ALT_INC2:
			out->m_slice_order = ALT_SHFT;
			for(int ii=out->m_slice_start+1; ii<=out->m_slice_end; ii+=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
			for(int ii=out->m_slice_start; ii<=out->m_slice_end; ii+=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
		break;
		case NIFTI_SLICE_ALT_DEC2:
			out->m_slice_order = RALT_SHFT;
			for(int ii=out->m_slice_end-1; ii>=out->m_slice_start; ii-=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
			for(int ii=out->m_slice_end; ii>=out->m_slice_start; ii-=2)
				out->m_slice_timing[ii] = ii*out->m_slice_duration;
		break;
	}

	return out;
}

int readNifti2Header(gzFile file, nifti2_header* header, bool* doswap, 
		bool verbose)
{
	// seek to 0
	gzseek(file, 0, SEEK_SET);

	static_assert(sizeof(nifti2_header) == 540, "Error, nifti header packing failed");

	// read header
	gzread(file, header, sizeof(nifti2_header));
	std::cerr << header->magic << std::endl;
	if(strncmp(header->magic, "n+2", 3)) {
		gzclearerr(file);
		gzrewind(file);
		return -1;
	}

	// byte swap
	int64_t npixel = 1;
	std::cerr << header->sizeof_hdr << endl;
	if(header->sizeof_hdr != 540) {
		*doswap = true;
		swap(&header->sizeof_hdr);
		if(header->sizeof_hdr != 540) {
			std::cerr << "Swapped Header Size: " << header->sizeof_hdr << std::endl;
			swap(&header->sizeof_hdr);
			std::cerr << "UnSwapped Header Size: " << header->sizeof_hdr << std::endl;
			std::cerr << "Malformed nifti input" << std::endl;
			return -1;
		}
		swap(&header->datatype);
		swap(&header->bitpix);
		swap(&header->ndim);
		for(size_t ii=0; ii<7; ii++)
			swap(&header->dim[ii]);
		swap(&header->intent_p1);
		swap(&header->intent_p2);
		swap(&header->intent_p3);
		swap(&header->qfac);
		for(size_t ii=0; ii<7; ii++)
			swap(&header->pixdim[ii]);
		swap(&header->vox_offset);
		swap(&header->scl_slope);
		swap(&header->scl_inter);
		swap(&header->cal_max);
		swap(&header->cal_min);
		swap(&header->slice_duration);
		swap(&header->toffset);
		swap(&header->slice_start);
		swap(&header->slice_end);
//		swap(&header->glmax);
//		swap(&header->glmin);
		swap(&header->qform_code);
		swap(&header->sform_code);
		
		for(size_t ii=0; ii<3; ii++)
			swap(&header->quatern[ii]);
		for(size_t ii=0; ii<3; ii++)
			swap(&header->qoffset[ii]);
		for(size_t ii=0; ii<12; ii++)
			swap(&header->saffine[ii]);
		
		swap(&header->slice_code);
		swap(&header->xyzt_units);
		swap(&header->intent_code);

		for(int32_t ii=0; ii<header->ndim; ii++)
			npixel *= header->dim[ii];
	}
	
	if(verbose) {
		std::cerr << "sizeof_hdr=" << header->sizeof_hdr << std::endl;
		std::cerr << "magic =" << header->magic  << std::endl;
		std::cerr << "datatype=" << header->datatype << std::endl;
		std::cerr << "bitpix=" << header->bitpix << std::endl;
		std::cerr << "ndim=" << header->ndim << std::endl;
		for(size_t ii=0; ii < 7; ii++)
			std::cerr << "dim["<<ii<<"]=" << header->dim[ii] << std::endl;
		std::cerr << "intent_p1 =" << header->intent_p1  << std::endl;
		std::cerr << "intent_p2 =" << header->intent_p2  << std::endl;
		std::cerr << "intent_p3 =" << header->intent_p3  << std::endl;
		std::cerr << "qfac=" << header->qfac << std::endl;
		for(size_t ii=0; ii < 7; ii++)
			std::cerr << "pixdim["<<ii<<"]=" << header->pixdim[ii] << std::endl;
		std::cerr << "vox_offset=" << header->vox_offset << std::endl;
		std::cerr << "scl_slope =" << header->scl_slope  << std::endl;
		std::cerr << "scl_inter =" << header->scl_inter  << std::endl;
		std::cerr << "cal_max=" << header->cal_max << std::endl;
		std::cerr << "cal_min=" << header->cal_min << std::endl;
		std::cerr << "slice_duration=" << header->slice_duration << std::endl;
		std::cerr << "toffset=" << header->toffset << std::endl;
		std::cerr << "slice_start=" << header->slice_start << std::endl;
		std::cerr << "slice_end=" << header->slice_end << std::endl;
		std::cerr << "descrip=" << header->descrip << std::endl;
		std::cerr << "aux_file=" << header->aux_file << std::endl;
		std::cerr << "qform_code =" << header->qform_code  << std::endl;
		std::cerr << "sform_code =" << header->sform_code  << std::endl;
		for(size_t ii=0; ii < 3; ii++){
			std::cerr << "quatern["<<ii<<"]=" 
				<< header->quatern[ii] << std::endl;
		}
		for(size_t ii=0; ii < 3; ii++){
			std::cerr << "qoffset["<<ii<<"]=" 
				<< header->qoffset[ii] << std::endl;
		}
		for(size_t ii=0; ii < 3; ii++) {
			for(size_t jj=0; jj < 4; jj++) {
				std::cerr << "saffine["<<ii<<"*4+"<<jj<<"]=" 
					<< header->saffine[ii*4+jj] << std::endl;
			}
		}
		std::cerr << "slice_code=" << header->slice_code << std::endl;
		std::cerr << "xyzt_units=" << header->xyzt_units << std::endl;
		std::cerr << "intent_code =" << header->intent_code  << std::endl;
		std::cerr << "intent_name=" << header->intent_name << std::endl;
		std::cerr << "dim_info.bits.freqdim=" << header->dim_info.bits.freqdim << std::endl;
		std::cerr << "dim_info.bits.phasedim=" << header->dim_info.bits.phasedim << std::endl;
		std::cerr << "dim_info.bits.slicedim=" << header->dim_info.bits.slicedim << std::endl;
		std::cerr << "unused_str=" << header->unused_str << std::endl;
	}
	
	return 0;
}

ostream& operator<<(ostream &out, const MRImage& img)
{
	out << "---------------------------" << endl;
	out << img.ndim() << "D Image" << endl;
	for(int64_t ii=0; ii<(int64_t)img.ndim(); ii++) {
		out << "dim[" << ii << "]=" << img.dim(ii);
		if(img.m_freqdim == ii) 
			out << " (frequency-encode)";
		if(img.m_phasedim == ii) 
			out << " (phase-encode)";
		if(img.m_slicedim == ii) 
			out << " (slice-encode)";
		out << endl;
	}

	out << "Direction: " << endl;
	for(size_t ii=0; ii<img.ndim(); ii++) {
		cerr << "[ ";
		for(size_t jj=0; jj<img.ndim(); jj++) {
			cerr << std::setw(10) << std::setprecision(3) << img.direction()(ii,jj);
		}
		cerr << "] " << endl;
	}
	
	out << "Spacing: " << endl;
	for(size_t ii=0; ii<img.ndim(); ii++) {
		out << "[ " << std::setw(10) << std::setprecision(3) 
			<< img.space()[ii] << "] ";
	}
	out << endl;

	out << "Origin: " << endl;
	for(size_t ii=0; ii<img.ndim(); ii++) {
		out << "[ " << std::setw(10) << std::setprecision(3) 
			<< img.origin()[ii] << "] ";
	}
	out << endl;
	
	out << "Affine: " << endl;
	for(size_t ii=0; ii<img.ndim()+1; ii++) {
		cerr << "[ ";
		for(size_t jj=0; jj<img.ndim()+1; jj++) {
			cerr << std::setw(10) << std::setprecision(3) << img.affine()(ii,jj);
		}
		cerr << "] " << endl;
	}

	switch(img.type()) {
		case UNKNOWN_TYPE:
		case UINT8:
			out << "UINT8" << endl;
			break;
		case INT16:
			out << "INT16" << endl;
			break;
		case INT32:
			out << "INT32" << endl;
			break;
		case FLOAT32:
			out << "FLOAT32" << endl;
			break;
		case COMPLEX64:
			out << "COMPLEX64" << endl;
			break;
		case FLOAT64:
			out << "FLOAT64" << endl;
			break;
		case RGB24:
			out << "RGB24" << endl;
			break;
		case INT8:
			out << "INT8" << endl;
			break;
		case UINT16:
			out << "UINT16" << endl;
			break;
		case UINT32:
			out << "UINT32" << endl;
			break;
		case INT64:
			out << "INT64" << endl;
			break;
		case UINT64:
			out << "UINT64" << endl;
			break;
		case FLOAT128:
			out << "FLOAT128" << endl;
			break;
		case COMPLEX128:
			out << "COMPLEX128" << endl;
			break;
		case COMPLEX256:
			out << "COMPLEX256" << endl;
			break;
		case RGBA32:
			out << "RGBA32" << endl;
			break;
		default:
			out << "UNKNOWN" << endl;
			break;
	}

	out << "Slice Duration: " << img.m_slice_duration << endl;
	out << "Slice Start: " << img.m_slice_start << endl;
	out << "Slice End: " << img.m_slice_end << endl;
	switch(img.m_slice_order) {
		case SEQ:
			out << "Slice Order: Increasing Sequential" << endl;
			break;
		case RSEQ:
			out << "Slice Order: Decreasing Sequential" << endl;
			break;
		case ALT: 
			out << "Slice Order: Increasing Alternating" << endl;
			break;
		case RALT:
			out << "Slice Order: Decreasing Alternating" << endl;
			break;
		case ALT_SHFT: 
			out << "Slice Order: Alternating Starting at " 
				<< img.m_slice_start+1 << " (not " 
				<< img.m_slice_start << ")" << endl;
			break;
		case RALT_SHFT:
			out << "Slice Order: Decreasing Alternating Starting at " 
				<< img.m_slice_end-1 << " not ( " << img.m_slice_end << endl;
			break;
		case UNKNOWN_SLICE:
		default:
			out << "Slice Order: Not Set" << endl;
			break;
	}
	out << "Slice Timing: " << endl;
	for(size_t ii=0; ii<img.m_slice_timing.size(); ii++) {
		out << std::setw(10) << std::setprecision(3) 
			<< img.m_slice_timing[ii] << ",";
	}
	out << endl;
	out << "---------------------------" << endl;
	return out;
}


} // npl
