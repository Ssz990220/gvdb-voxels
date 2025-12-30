//-----------------------------------------------------------------------------
// NVIDIA(R) GVDB VOXELS
// Copyright 2017 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0
//
// Version 1.0: Rama Hoetzlein, 5/1/2017
//-----------------------------------------------------------------------------

#include "gvdb.h"
using namespace nvdb;

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "file_png.h"		// sample utils

VolumeGVDB gvdb;

void print_usage(const char* prog) {
	printf("Usage: %s [options]\n", prog);
	printf("Options:\n");
	printf("  -i, --input <file.vdb>    Input VDB file (default: bunny.vdb)\n");
	printf("  -o, --output <file.png>   Output PNG file (default: none, display only)\n");
	printf("  -v, --vbx <file.vbx>      Output VBX file (default: none)\n");
	printf("  -d, --display             Display image using external viewer\n");
	printf("  -h, --help                Show this help\n");
}

int main (int argc, char** argv)
{
	int w = 1024, h = 768;
	std::string input_vdb = "bunny.vdb";
	std::string output_png = "";
	std::string output_vbx = "";
	bool display_image = false;

	// Parse command line arguments
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
			if (i + 1 < argc) input_vdb = argv[++i];
		} else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
			if (i + 1 < argc) output_png = argv[++i];
		} else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--vbx") == 0) {
			if (i + 1 < argc) output_vbx = argv[++i];
		} else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--display") == 0) {
			display_image = true;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			print_usage(argv[0]);
			return 0;
		}
	}

	// Initialize GVDB
	printf ( "Starting GVDB.\n" );
#ifdef _DEBUG
	gvdb.SetDebug(true);
#endif // #ifdef _DEBUG
	gvdb.SetVerbose ( true );		// enable/disable console output from gvdb
	gvdb.SetCudaDevice ( GVDB_DEV_FIRST );
	gvdb.Initialize ();
	gvdb.AddPath ( "../source/shared_assets/" );
	gvdb.AddPath ( ASSET_PATH );

	// Load VDB
	char scnpath[1024];
	if ( !gvdb.getScene()->FindFile ( input_vdb.c_str(), scnpath ) ) {
		gprintf ( "Cannot find vdb file: %s\n", input_vdb.c_str() );
		gerror();
	}
	printf ( "Loading VDB. %s\n", scnpath );
	gvdb.SetChannelDefault ( 16, 16, 1 );
	if ( !gvdb.LoadVDB ( scnpath ) ) {                	// Load OpenVDB format
		gerror();
	}

	// Save VBX if requested
	if (!output_vbx.empty()) {
		printf("Saving VBX to: %s\n", output_vbx.c_str());
		gvdb.SaveVBX ( output_vbx.c_str() );
	}

	// Set volume params
	gvdb.getScene()->SetSteps ( 0.25f, 16, 0.25f );			// Set raycasting steps per voxel
	gvdb.getScene()->SetExtinct ( -1.0f, 1.5f, 0.0f );		// Set volume extinction
	gvdb.getScene()->SetVolumeRange ( 0.0f, 1.0f, -1.0f );	// Set volume value range (for a level set)
	gvdb.getScene()->SetCutoff ( 0.005f, 0.01f, 0.0f );
	gvdb.getScene()->LinearTransferFunc ( 0.00f, 0.25f, Vector4DF(1,1,0,0.05f), Vector4DF(1,1,0,0.03f) );
	gvdb.getScene()->LinearTransferFunc ( 0.25f, 0.50f, Vector4DF(1,1,1,0.03f), Vector4DF(1,0,0,0.02f) );
	gvdb.getScene()->LinearTransferFunc ( 0.50f, 0.75f, Vector4DF(1,0,0,0.02f), Vector4DF(1,.5f,0,0.01f) );
	gvdb.getScene()->LinearTransferFunc ( 0.75f, 1.00f, Vector4DF(1,.5f,0,0.01f), Vector4DF(0,0,0,0.005f) );
	gvdb.getScene()->SetBackgroundClr ( 0, 0, 0, 1 );
	gvdb.SetEpsilon(0.01f, 256);
	gvdb.CommitTransferFunc ();


	Camera3D* cam = new Camera3D;						// Create Camera
	cam->setFov ( 30.0 );
	cam->setOrbit ( Vector3DF(-10,30,0), Vector3DF(14.2f,15.3f,18.0f), 130, 1.0f );
	gvdb.getScene()->SetCamera( cam );
	gvdb.getScene()->SetRes ( w, h );

	Light* lgt = new Light;								// Create Light
	lgt->setOrbit ( Vector3DF(30,50,0), Vector3DF(15,15,15), 200, 1.0 );
	gvdb.getScene()->SetLight ( 0, lgt );

	printf ( "Creating screen buffer. %d x %d\n", w, h );
	gvdb.AddRenderBuf ( 0, w, h, 4 );					// Add render buffer

	gvdb.TimerStart ();
	gvdb.Render ( SHADE_LEVELSET, 0, 0 );			// Render as volume
	float rtime = gvdb.TimerStop();
	printf ( "Render volume. %6.3f ms\n", rtime );

	unsigned char* buf = (unsigned char*) malloc ( w*h*4 );
	gvdb.ReadRenderBuf ( 0, buf );						// Read render buffer

	// Save PNG if output path specified
	if (!output_png.empty()) {
		printf ( "Writing %s\n", output_png.c_str() );
		save_png ( const_cast<char*>(output_png.c_str()), buf, w, h, 4 );
	}

	// Display image if requested
	if (display_image) {
		// Save to temp file and display
		char tmp_png[] = "/tmp/gvdb_render.png";
		save_png ( tmp_png, buf, w, h, 4 );
		printf("Displaying image...\n");
		std::string cmd = std::string("xdg-open ") + tmp_png + " 2>/dev/null &";
		system(cmd.c_str());
	}

	free ( buf );
	// Note: cam and lgt are managed by gvdb.getScene(), don't delete them manually

	gprintf ( "Done.\n" );
	return 0;
}
