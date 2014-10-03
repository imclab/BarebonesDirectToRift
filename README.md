BarebonesDirectToRift
=====================

Demonstrates minimum code required for Direct-to-Rift display to work with a D3D11 device.

The basic requirements seem to be:

 * Call ovr_InitializeRenderingShim or ovr_Initialize before creating the graphics device
 * Call ovrHmd_AttachToWindow
 * Call ovrHmd_GetTrackingState or ovrHmd_GetEyePose, possibly on the main window thread

You should be able to drop this repo into the "Samples" directory of the OVR 0.4.2 SDK and have it just work.
