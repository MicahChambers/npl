/******************************************************************************
 * Copyright 2014 Micah C Chambers (micahc.vt@gmail.com)
 *
 * NPL is free software: you can redistribute it and/or modify it under the
 * terms of the BSD 2-Clause License available in LICENSE or at
 * http://opensource.org/licenses/BSD-2-Clause
 *
 * @file fmri_gica2.cpp New version of group ICA that performs two SVD's to
 * reduce the memory footprint, prior to performing ICA.
 *
 *****************************************************************************/

#include <tclap/CmdLine.h>
#include <string>
#include <stdexcept>

#include <Eigen/Dense>
#include <Eigen/IterativeSolvers>

#include "version.h"
#include "fmri_inference.h"
#include "mrimage.h"
#include "nplio.h"
#include "iterators.h"
#include "statistics.h"
#include "macros.h"

using std::string;
using std::shared_ptr;
using std::to_string;

using namespace Eigen;
using namespace npl;

TCLAP::MultiSwitchArg a_verbose("v", "verbose", "Write lots of output. "
		"Be wary for large datasets!", 1);

int main(int argc, char** argv)
{
	cerr << "Version: " << __version__ << endl;
	try {
	/*
	 * Command Line
	 */

	TCLAP::CmdLine cmd("Perform ICA analysis on an image, or group of images. "
			"By default this will be a temporal ICA. Group analysis is "
			"done by concatinating in time. By default a Temporal ICA will "
			"be performed, to do a spatial ICA select -S.", ' ', __version__ );

	TCLAP::ValueArg<string> a_reorgprefix("", "reorg-prefix", "Prefix for input tall "
			"matrices and masks, used in nplGICA_reorg", true, "", "prefix", cmd);
	TCLAP::ValueArg<string> a_reduceprefix("", "reduce-prefix", "Prefix for reduced "
			"(using SVD) matrices, used in nplGICA_reduce", true, "", "prefix", cmd);
	TCLAP::ValueArg<string> a_icaprefix("", "ica-prefix", "Prefix for output "
			"ICA matrices.", true, "", "prefix", cmd);

	TCLAP::SwitchArg a_temporal_ica("T", "temporal-ica", "Perform a temporal ICA"
			", producing temporally independent "
			"time-series times a spatial unmixing matrix.", cmd);

	cmd.add(a_verbose);
	cmd.parse(argc, argv);

	if(a_temporal_ica.isSet()) {
		gicaTemporalICA(a_reorgprefix.getValue(), a_reduceprefix.getValue(),
				a_icaprefix.getValue(), a_verbose.isSet());
	} else {
		gicaSpatialICA(a_reorgprefix.getValue(), a_reduceprefix.getValue(),
				a_icaprefix.getValue(), a_verbose.isSet());
	}


	} catch (TCLAP::ArgException &e)  // catch any exceptions
	{ std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; }

	return 0;
}



